#include "data.hpp"
#include "gl-handle.hpp"
#include <fmt/ranges.h>
#include "csv.hpp"
#include <array>
#include "settings.hpp"
#include "overload.hpp"
#include "range_setting.hpp"
#include <algorithm>
#include <ranges>
#include <string_view>
#include "program.hpp"
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <map>
#include "user_definitions.hpp"

using namespace std::literals;

namespace
{
using namespace explot;

std::string to_glsl(const expr &e, std::span<const int> indices = {})
{
  namespace views = std::ranges::views;
  struct visitor
  {
    std::span<const int> indices;
    std::string operator()(const literal_expr &l) const { return std::to_string(l.value); }

    std::string operator()(const var &v) const { return v.name; }

    std::string operator()(const box<unary_builtin_call> &call) const
    {
      return fmt::format("{}({})", call->name, std::visit(*this, call->arg));
    }

    std::string operator()(const box<binary_builtin_call> &call) const
    {
      return fmt::format("{}({}, {})", call->name, std::visit(*this, call->arg1),
                         std::visit(*this, call->arg2));
    }

    std::string operator()(data_ref d) const
    {
      if (d.idx == 0)
      {
        return "gl_VertexID";
      }
      const auto idx = std::ranges::find(indices, d.idx);
      const auto pos = std::distance(indices.begin(), idx);
      assert(idx != indices.end());
      return fmt::format("row[{}]", pos);
    }

    std::string operator()(const box<unary_op> &op) const
    {
      auto o = op->op == unary_operator::minus ? '-' : '+';
      return fmt::format("{}({})", o, std::visit(*this, op->operand));
    }

    std::string operator()(const box<binary_op> &op) const
    {
      auto o = [&]()
      {
        switch (op->op)
        {
        case binary_operator::plus:
          return '+';
        case binary_operator::minus:
          return '-';
        case binary_operator::mult:
          return '*';
        case binary_operator::div:
          return '/';
        }
      }();
      return fmt::format("({}) {} ({})", std::visit(*this, op->lhs), o, std::visit(*this, op->rhs));
    }

    std::string operator()(const user_var_ref &ref) const { return get_definition(ref.idx).name; }

    std::string operator()(const box<user_function_call> &call) const
    {
      return fmt::format("{}({})", get_definition(call->idx).name,
                         fmt::join(call->args
                                       | std::views::transform([this](const expr &arg)
                                                               { return std::visit(*this, arg); }),
                                   ", "));
    }
  };
  return std::visit(visitor{indices}, e);
}

std::string glsl_definitions(std::span<const uint32_t> defs)
{
  std::string result;

  for (auto idx : defs)
  {
    const auto &def = get_definition(idx);
    if (def.params)
    {
      fmt::format_to(std::back_inserter(result), "float {}({}) {{ return {}; }}", def.name,
                     fmt::join(def.params.value()
                                   | std::views::transform([](const std::string &p)
                                                           { return fmt::format("float {}", p); }),
                               ", "),
                     to_glsl(def.body));
    }
    else
    {
      fmt::format_to(std::back_inserter(result), "float {} = {};", def.name, to_glsl(def.body));
    }
  }
  return result;
}

void extract_user_refs(const expr &e, std::vector<uint32_t> &refs)
{
  struct visitor
  {
    std::vector<uint32_t> &refs;

    void operator()(const literal_expr &) {}
    void operator()(const box<unary_op> &op) { std::visit(*this, op->operand); }
    void operator()(const box<binary_op> &op)
    {
      std::visit(*this, op->lhs);
      std::visit(*this, op->rhs);
    }
    void operator()(const box<unary_builtin_call> &call) { std::visit(*this, call->arg); }
    void operator()(const box<binary_builtin_call> &call)
    {
      std::visit(*this, call->arg1);
      std::visit(*this, call->arg2);
    }
    void operator()(const box<user_function_call> &call) { refs.push_back(call->idx); }
    void operator()(const user_var_ref &ref) { refs.push_back(ref.idx); }
    void operator()(const var &) {}
    void operator()(const data_ref) {}
  };

  std::visit(visitor{refs}, e);
}

std::vector<uint32_t> extract_user_refs(std::span<const expr> exprs)
{
  std::vector<uint32_t> result;
  for (const auto &e : exprs)
  {
    extract_user_refs(e, result);
  }
  std::ranges::sort(result);
  result.erase(std::ranges::unique(result).begin(), result.end());
  return result;
}

std::string glsl_definitions(std::span<const expr> exprs)
{
  return glsl_definitions(extract_user_refs(exprs));
}

program_handle program_for_expressions(const char *shader_source_fmt, std::span<const expr> exprs,
                                       std::span<const int> indices = {})
{
  // Not the most efficient code. But exprs.size() < 10, so it should be ok
  auto assignments_str = std::string();
  auto varyings_str = std::string();
  auto varyings_ids = std::vector<std::string>();
  varyings_ids.reserve(exprs.size());
  auto varyings_ptrs = std::vector<const char *>();
  varyings_ptrs.reserve(exprs.size());

  for (auto i = 0u; i < exprs.size(); ++i)
  {
    fmt::format_to(std::back_inserter(varyings_str), "out float v{};\n", i);
    fmt::format_to(std::back_inserter(assignments_str), "v{} = {};\n", i,
                   to_glsl(exprs[i], indices));

    varyings_ids.push_back(fmt::format("v{}", i));
    varyings_ptrs.push_back(varyings_ids.back().c_str());
  }

  auto prelude = varyings_str + glsl_definitions(exprs);

  auto shader_src = fmt::format(fmt::runtime(shader_source_fmt), prelude, assignments_str);
  // fmt::println("{}", shader_src);
  return make_program_with_varying(shader_src.c_str(), varyings_ptrs);
}

program_handle program_for_using_expressions(std::span<const expr> exprs,
                                             std::span<const int> indices)
{
  static constexpr char shader_source_fmt[] = R"(#version 330 core
layout(location = 0) in float row[{}];

{{}}

void main()
{{{{
{{}}
}}}}
)";

  auto shader_src = fmt::format(shader_source_fmt, std::max(1UL, indices.size()));
  return program_for_expressions(shader_src.c_str(), exprs, indices);
}

program_handle program_for_functional_data_2d(std::span<const expr> exprs)
{
  static constexpr auto shader_source_fmt = R"(#version 330 core

uniform float min_x;
uniform float step_x;

{}

void  main()
{{
  float x = min_x + step_x * gl_InstanceID;
  {}
}}
)";

  return program_for_expressions(shader_source_fmt, exprs);
}

program_handle program_for_parametric_data_2d(const expr (&exprs)[2])
{
  static constexpr auto shader_source_fmt = R"(#version 330 core

uniform float min_t;
uniform float step_t;

out vec2 v;

{}

void  main()
{{
  float t = min_t + step_t * gl_InstanceID;
  float x_value = {};
  float y_value = {};
  v = vec2(x_value, y_value);
}}
)";
  auto x_glsl = to_glsl(exprs[0]);
  auto y_glsl = to_glsl(exprs[1]);
  auto glsl_defs = glsl_definitions(exprs);
  auto shader_source = fmt::format(shader_source_fmt, glsl_defs, x_glsl, y_glsl);
  return make_program_with_varying(shader_source.c_str(), "v");
}

program_handle program_for_functional_data_3d_y(const expr &e)
{
  static constexpr auto shader_source_fmt = R"shader(#version 330 core

uniform int num_points_per_line;
uniform float min_x;
uniform float step_x;
uniform float min_y;
uniform float step_y;

out vec3 v;

{}

void  main()
{{
  float x = min_x + floor(gl_VertexID / num_points_per_line) * step_x;
  float y = min_y + (gl_VertexID % num_points_per_line) * step_y;
  float value = {};
  v = vec3(x, y, value);
}}
)shader";
  auto glsl = to_glsl(e);
  const auto shader_source = fmt::format(shader_source_fmt, glsl_definitions({&e, 1}), glsl);
  return make_program_with_varying(shader_source.c_str(), "v");
}

program_handle program_for_functional_data_3d_x(const expr &e)
{
  static constexpr auto shader_source_fmt = R"shader(#version 330 core

uniform int num_points_per_line;
uniform float min_x;
uniform float step_x;
uniform float min_y;
uniform float step_y;

out vec3 v;

{}

void  main()
{{
  float x = min_x + (gl_VertexID % num_points_per_line) * step_x;
  float y = min_y + floor(gl_VertexID / num_points_per_line) * step_y;
  float value = {};
  v = vec3(x, y, value);
}}
)shader";
  const auto glsl = to_glsl(e);
  const auto shader_source = fmt::format(shader_source_fmt, glsl_definitions({&e, 1}), glsl);
  return make_program_with_varying(shader_source.c_str(), "v");
}

program_handle program_for_parametric_data_3d_v(const expr (&exprs)[3])
{
  static constexpr auto shader_source_fmt = R"shader(#version 330 core

uniform int num_points_per_line;
uniform float min_u;
uniform float step_u;
uniform float min_v;
uniform float step_v;

out vec3 p;

{}

void  main()
{{
  float u = min_u + floor(gl_VertexID / num_points_per_line) * step_u;
  float v = min_v + (gl_VertexID % num_points_per_line) * step_v;
  float x = {};
  float y = {};
  float z = {};
  p = vec3(x, y, z);
}}
)shader";
  const auto x_glsl = to_glsl(exprs[0]);
  const auto y_glsl = to_glsl(exprs[1]);
  const auto z_glsl = to_glsl(exprs[2]);
  const auto glsl_defs = glsl_definitions(exprs);
  const auto shader_source = fmt::format(shader_source_fmt, glsl_defs, x_glsl, y_glsl, z_glsl);
  return make_program_with_varying(shader_source.c_str(), "p");
}

program_handle program_for_parametric_data_3d_u(const expr (&exprs)[3])
{
  static constexpr auto shader_source_fmt = R"shader(#version 330 core

uniform int num_points_per_line;
uniform float min_u;
uniform float step_u;
uniform float min_v;
uniform float step_v;

out vec3 p;

{}

void  main()
{{
  float u = min_u + (gl_VertexID % num_points_per_line) * step_u;
  float v = min_v + floor(gl_VertexID / num_points_per_line) * step_v;
  float x = {};
  float y = {};
  float z = {};
  p = vec3(x, y, z);
}}
)shader";
  const auto x_glsl = to_glsl(exprs[0]);
  const auto y_glsl = to_glsl(exprs[1]);
  const auto z_glsl = to_glsl(exprs[2]);
  const auto glsl_defs = glsl_definitions(exprs);
  const auto shader_source = fmt::format(shader_source_fmt, glsl_defs, x_glsl, y_glsl, z_glsl);
  return make_program_with_varying(shader_source.c_str(), "p");
}

void extract_indices(const expr &e, std::vector<int> &indices)
{
  struct visitor
  {
    std::vector<int> *indices;
    void operator()(const literal_expr &) {}
    void operator()(const var &) {}
    void operator()(const box<unary_builtin_call> &call) { std::visit(*this, call->arg); }
    void operator()(const box<binary_builtin_call> &call)
    {
      std::visit(*this, call->arg1);
      std::visit(*this, call->arg2);
    }
    void operator()(const box<unary_op> &u) { std::visit(*this, u->operand); }
    void operator()(const box<binary_op> &b)
    {
      std::visit(*this, b->lhs);
      std::visit(*this, b->rhs);
    }
    void operator()(const data_ref &d) { indices->push_back(d.idx); }
    void operator()(const user_var_ref &) {}
    void operator()(const box<user_function_call> &call)
    {
      for (const auto &arg : call->args)
      {
        std::visit(*this, arg);
      }
    }
  };
  std::visit(visitor{&indices}, e);
}

std::vector<int> extract_indices(std::span<const expr> expressions)
{
  std::vector<int> indices;
  for (const auto &e : expressions)
  {
    extract_indices(e, indices);
  }
  std::ranges::sort(indices);
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  if (indices.size() > 0 && indices[0] == 0)
  {
    indices.erase(indices.begin());
  }
  return indices;
}

std::vector<int> grid_indices_for_lines(uint32_t num_points, uint32_t num_columns)
{
  assert(num_points % num_columns == 0);
  auto num_rows = num_points / num_columns;
  auto result = std::vector<int>();
  result.reserve(2 * num_points);

  for (auto col = 0u; col < num_columns; ++col)
  {
    for (auto row = 0u; row < num_rows; ++row)
    {
      result.emplace_back(row * num_columns + col);
    }
  }

  for (auto row = 0u; row < num_rows; ++row)
  {
    for (auto col = 0u; col < num_columns; ++col)
    {
      result.emplace_back(row * num_columns + col);
    }
  }

  return result;
}

data_desc grid_data_for_lines(vbo_handle vbo, uint32_t num_points, uint32_t num_columns)
{
  auto indices = grid_indices_for_lines(num_points, num_columns);
  auto ebo = make_vbo();
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, ebo);
  glBufferData(GL_ARRAY_BUFFER, indices.size() * sizeof(GLint), indices.data(), GL_STATIC_DRAW);
  assert(num_points % num_columns == 0);
  auto num_rows = num_points / num_columns;
  auto count = std::vector<GLsizei>();
  count.reserve(num_columns + num_rows);
  std::fill_n(std::back_inserter(count), num_columns, num_rows);
  std::fill_n(std::back_inserter(count), num_rows, num_columns);
  return data_desc(std::move(vbo), 3, std::move(count), std::move(ebo), num_points);
}

struct row_data
{
  std::string filename;
  std::optional<uint32_t> columns;
  std::vector<int> indices;
  data_desc data;
};

std::pair<std::vector<row_data>, time_point>
row_data_for_graphs(const std::span<const graph_desc_2d> gs)
{
  auto files = std::unordered_map<std::string_view, std::vector<int>>();
  for (const auto &g : gs)
  {
    if (g.data.index() != 1)
    {
      continue;
    }
    else
    {
      const auto &d = std::get<1>(g.data);
      auto &indices = files[d.path];
      auto new_indices = extract_indices(d.expressions);
      indices.reserve(indices.size() + new_indices.size());
      std::ranges::copy(new_indices, std::back_inserter(indices));
    }
  }

  auto result = std::vector<row_data>();
  result.reserve(files.size());
  auto timebase = std::optional<time_point>();
  for (auto &[f, indices] : files)
  {
    std::ranges::sort(indices);
    indices.erase(std::ranges::unique(indices).begin(), indices.end());
    auto num_indices = indices.size();
    auto data = num_indices > 0 ? read_csv(f, settings::datafile::separator(), indices, timebase)
                                : std::vector<float>();
    assert(data.size() % num_indices == 0);
    auto num_points =
        static_cast<uint32_t>(num_indices > 0 ? data.size() / num_indices : count_lines(f));
    auto csv_vbo = make_vbo();
    if (num_indices > 0)
    {
      auto vao = make_vao();
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, csv_vbo);
      glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    }
    result.emplace_back(std::string(f), std::nullopt, std::move(indices),
                        data_desc(std::move(csv_vbo),
                                  std::max(1u, static_cast<uint32_t>(indices.size())), num_points));
  }
  return std::make_pair(std::move(result), timebase.value_or(time_point()));
}

std::pair<std::vector<row_data>, time_point>
row_data_for_graphs(const std::span<const graph_desc_3d> gs)
{
  // using ordered map, because there is no std::hash for tuple or pair
  auto files = std::map<std::tuple<std::string_view, bool>, std::vector<int>>();
  for (const auto &g : gs)
  {
    if (g.data.index() != 1)
    {
      continue;
    }
    else
    {
      const auto &d = std::get<1>(g.data);
      auto &indices = files[{d.path, d.matrix}];
      auto new_indices = extract_indices(d.expressions);
      indices.reserve(indices.size() + new_indices.size());
      std::ranges::copy(new_indices, std::back_inserter(indices));
    }
  }

  auto result = std::vector<row_data>();
  result.reserve(files.size());
  auto timebase = std::optional<time_point>();
  for (auto &[p, indices] : files)
  {
    auto &[f, matrix] = p;
    std::ranges::sort(indices);
    indices.erase(std::ranges::unique(indices).begin(), indices.end());
    auto num_indices = static_cast<uint32_t>(indices.size());
    if (matrix)
    {
      auto [data, columns] = read_matrix_csv(f, settings::datafile::separator(), timebase);
      assert(data.size() % 3 == 0);
      auto num_points = static_cast<uint32_t>(data.size() / 3);
      assert(num_points % columns == 0);
      auto csv_vbo = make_vbo();
      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, csv_vbo);
      glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
      result.emplace_back(std::string(f), columns, std::move(indices),
                          data_desc(std::move(csv_vbo), 3, num_points));
    }
    else
    {
      auto data = [&]
      {
        if (num_indices == 0)
        {
          return std::vector<float>();
        }
        else
        {
          return read_csv(f, settings::datafile::separator(), indices, timebase);
        }
      }();
      assert(data.size() % num_indices == 0);
      auto num_points =
          static_cast<uint32_t>(num_indices > 0 ? data.size() / num_indices : count_lines(f));
      auto csv_vbo = make_vbo();
      if (num_indices > 0)
      {
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, csv_vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
      }
      result.emplace_back(std::string(f), std::nullopt, std::move(indices),
                          data_desc(std::move(csv_vbo), std::max(1u, num_indices), num_points));
    }
  }
  return std::make_pair(std::move(result), timebase.value_or(time_point()));
}

vbo_handle data_for_using_expressions(std::span<const expr> exprs, const row_data &r)
{
  const auto num_indices = r.indices.size();
  const auto num_points = r.data.num_points;
  auto vao = make_vao();
  glBindVertexArray(vao);
  auto &csv_vbo = r.data.vbo;
  glBindBuffer(GL_ARRAY_BUFFER, csv_vbo);
  for (auto i = 0U; i < num_indices; ++i)
  {
    glEnableVertexAttribArray(i);
    glVertexAttribPointer(i, 1, GL_FLOAT, GL_FALSE, num_indices * sizeof(float),
                          (void *)(i * sizeof(float)));
  }
  auto data_vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, data_vbo);
  glBufferData(GL_ARRAY_BUFFER, exprs.size() * num_points * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  auto program = program_for_using_expressions(exprs, r.indices);
  glUseProgram(program);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data_vbo);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(num_points));
  glEndTransformFeedback();
  return data_vbo;
}

data_desc data_for_expression_2d(mark_type_2d m, const expr &e, uint32_t num_points,
                                 range_setting xrange)
{
  auto min_x = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          xrange.lower_bound.value_or(-10.0f));
  auto max_x = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          xrange.upper_bound.value_or(10.0f));
  const auto step_x = (max_x - min_x) / static_cast<float>(num_points - 1);
  auto vao = make_vao();
  auto exprs = std::vector<expr>{var("x"), e};
  auto program = program_for_functional_data_2d(exprs);
  auto vbo = make_vbo();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, exprs.size() * num_points * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  glUseProgram(program);
  glUniform1f(glGetUniformLocation(program, "min_x"), min_x);
  glUniform1f(glGetUniformLocation(program, "step_x"), step_x);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArraysInstanced(GL_POINTS, 0, 1, num_points);
  glEndTransformFeedback();
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

  return data_desc(std::move(vbo), static_cast<uint32_t>(exprs.size()), num_points);
}

data_desc data_for_parametric_2d(const expr (&exprs)[2], uint32_t num_points, range_setting trange)
{
  auto min_t = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          trange.lower_bound.value_or(-10.0f));
  auto max_t = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          trange.upper_bound.value_or(10.0f));
  const auto step_t = (max_t - min_t) / static_cast<float>(num_points - 1);
  auto vao = make_vao();
  auto program = program_for_parametric_data_2d(exprs);
  auto vbo = make_vbo();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 2 * num_points * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
  glUseProgram(program);
  glUniform1f(glGetUniformLocation(program, "min_t"), min_t);
  glUniform1f(glGetUniformLocation(program, "step_t"), step_t);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArraysInstanced(GL_POINTS, 0, 1, num_points);
  glEndTransformFeedback();
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

  return data_desc(std::move(vbo), 2, num_points);
}

data_desc data_for_expression_3d(const expr &expr, samples_setting isosamples,
                                 samples_setting samples, range_setting xrange,
                                 range_setting yrange)
{
  auto min_x = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          xrange.lower_bound.value_or(-10.0f));
  auto max_x = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          xrange.upper_bound.value_or(10.0f));
  auto min_y = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          yrange.lower_bound.value_or(-10.0f));
  auto max_y = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          yrange.upper_bound.value_or(10.0f));
  assert(min_x < max_x && min_y < max_y);
  auto program_x = program_for_functional_data_3d_x(expr);
  auto program_y = program_for_functional_data_3d_y(expr);
  glUseProgram(program_x);
  const auto line_step_x = (max_x - min_x) / static_cast<float>(isosamples.x - 1);
  const auto point_step_x = (max_x - min_x) / static_cast<float>(samples.x - 1);
  const auto line_step_y = (max_y - min_y) / static_cast<float>(isosamples.y - 1);
  const auto point_step_y = (max_y - min_y) / static_cast<float>(samples.y - 1);
  const auto num_points_x = isosamples.y * samples.x;
  const auto num_points_y = isosamples.x * samples.y;
  const auto num_points = num_points_x + num_points_y;
  auto vao = make_vao();
  auto vbo = make_vbo();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, num_points * sizeof(glm::vec3), nullptr, GL_STATIC_DRAW);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, 0, num_points_x * sizeof(glm::vec3));
  glBeginTransformFeedback(GL_POINTS);
  glUniform1i(glGetUniformLocation(program_x, "num_points_per_line"), samples.x);
  glUniform1f(glGetUniformLocation(program_x, "min_x"), min_x);
  glUniform1f(glGetUniformLocation(program_x, "step_x"), point_step_x);
  glUniform1f(glGetUniformLocation(program_x, "min_y"), min_y);
  glUniform1f(glGetUniformLocation(program_x, "step_y"), line_step_y);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(num_points_x));
  glEndTransformFeedback();
  glUseProgram(program_y);
  glUniform1i(glGetUniformLocation(program_y, "num_points_per_line"), samples.y);
  glUniform1f(glGetUniformLocation(program_y, "min_x"), min_x);
  glUniform1f(glGetUniformLocation(program_y, "step_x"), line_step_x);
  glUniform1f(glGetUniformLocation(program_y, "min_y"), min_y);
  glUniform1f(glGetUniformLocation(program_y, "step_y"), point_step_y);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, num_points_x * sizeof(glm::vec3),
                    num_points_y * sizeof(glm::vec3));
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(num_points_y));
  glEndTransformFeedback();
  auto count = std::vector<GLsizei>();
  count.reserve(isosamples.x + isosamples.y);
  std::fill_n(std::back_inserter(count), isosamples.y, samples.x);
  std::fill_n(std::back_inserter(count), isosamples.x, samples.y);
  return data_desc(std::move(vbo), 3, std::move(count));
}

data_desc data_for_parametric_3d(const expr (&exprs)[3], samples_setting isosamples,
                                 samples_setting samples, range_setting u_range,
                                 range_setting v_range)
{
  auto min_u = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          u_range.lower_bound.value_or(-10.0f));
  auto max_u = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          u_range.upper_bound.value_or(10.0f));
  auto min_v = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          v_range.lower_bound.value_or(-10.0f));
  auto max_v = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          v_range.upper_bound.value_or(10.0f));
  assert(min_u < max_u && min_v < max_v);
  auto program_x = program_for_parametric_data_3d_u(exprs);
  auto program_y = program_for_parametric_data_3d_v(exprs);
  glUseProgram(program_x);
  const auto line_step_u = (max_u - min_u) / static_cast<float>(isosamples.x - 1);
  const auto point_step_u = (max_u - min_u) / static_cast<float>(samples.x - 1);
  const auto line_step_v = (max_v - min_v) / static_cast<float>(isosamples.y - 1);
  const auto point_step_v = (max_v - min_v) / static_cast<float>(samples.y - 1);
  const auto num_points_u = isosamples.y * samples.x;
  const auto num_points_v = isosamples.x * samples.y;
  const auto num_points = num_points_u + num_points_v;
  auto vao = make_vao();
  auto vbo = make_vbo();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, num_points * sizeof(glm::vec3), nullptr, GL_STATIC_DRAW);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, 0, num_points_u * sizeof(glm::vec3));
  glBeginTransformFeedback(GL_POINTS);
  glUniform1i(glGetUniformLocation(program_x, "num_points_per_line"), samples.x);
  glUniform1f(glGetUniformLocation(program_x, "min_u"), min_u);
  glUniform1f(glGetUniformLocation(program_x, "step_u"), point_step_u);
  glUniform1f(glGetUniformLocation(program_x, "min_v"), min_v);
  glUniform1f(glGetUniformLocation(program_x, "step_v"), line_step_v);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(num_points_u));
  glEndTransformFeedback();
  glUseProgram(program_y);
  glUniform1i(glGetUniformLocation(program_y, "num_points_per_line"), samples.y);
  glUniform1f(glGetUniformLocation(program_y, "min_u"), min_u);
  glUniform1f(glGetUniformLocation(program_y, "step_u"), line_step_u);
  glUniform1f(glGetUniformLocation(program_y, "min_v"), min_v);
  glUniform1f(glGetUniformLocation(program_y, "step_v"), point_step_v);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, num_points_u * sizeof(glm::vec3),
                    num_points_v * sizeof(glm::vec3));
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(num_points_v));
  glEndTransformFeedback();
  auto count = std::vector<GLsizei>();
  count.reserve(isosamples.x + isosamples.y);
  std::fill_n(std::back_inserter(count), isosamples.y, samples.x);
  std::fill_n(std::back_inserter(count), isosamples.x, samples.y);
  return data_desc(std::move(vbo), 3, std::move(count));
}

} // namespace

namespace explot
{

data_desc data_for_span(std::span<const float> data, size_t stride)
{
  assert(data.size() % stride == 0);
  auto vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * data.size(), data.data(), GL_DYNAMIC_DRAW);
  return {std::move(vbo), 1, static_cast<std::uint32_t>(data.size() / stride)};
}

data_desc data_for_span(std::span<const glm::vec3> data)
{
  auto vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), data.data(), GL_DYNAMIC_DRAW);
  return {std::move(vbo), 3, static_cast<std::uint32_t>(data.size())};
}

data_desc::data_desc(vbo_handle vbo, std::uint32_t point_size, std::uint32_t num_points,
                     std::uint32_t num_segments)
    : data_desc(std::move(vbo), point_size,
                std::vector<GLsizei>(num_segments, static_cast<GLsizei>(num_points / num_segments)))
{
  assert(num_points % num_segments == 0);
}

data_desc::data_desc(vbo_handle vbo, std::uint32_t point_size, std::vector<GLsizei> count)
    : vbo(std::move(vbo)), ebo(make_vbo()),
      num_points(static_cast<uint32_t>(std::ranges::fold_left(count, 0, std::plus<GLsizei>()))),
      num_indices(num_points), point_size(point_size), count(std::move(count))
{
  starts.reserve(this->count.size());
  auto start = uintptr_t(0);
  for (const auto &c : this->count)
  {
    starts.push_back(start * sizeof(GLuint));
    start += static_cast<uintptr_t>(c);
  }
  glBindVertexArray(0);
  auto indices = std::make_unique_for_overwrite<GLuint[]>(num_points);
  for (auto i = 0u; i < num_points; ++i)
  {
    indices[i] = i;
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_points * sizeof(GLuint), indices.get(), GL_STATIC_DRAW);
}

data_desc::data_desc(vbo_handle vbo, uint32_t point_size, std::vector<GLsizei> count,
                     vbo_handle ebo, uint32_t num_points)
    : vbo(std::move(vbo)), ebo(std::move(ebo)), num_points(num_points),
      num_indices(static_cast<uint32_t>(std::ranges::fold_left(count, 0, std::plus<GLsizei>()))),
      count(std::move(count))
{
  starts.reserve(this->count.size());
  auto start = uintptr_t(0);
  for (const auto &c : this->count)
  {
    starts.push_back(start * sizeof(GLuint));
    start += static_cast<uintptr_t>(c);
  }
}

void print_data(const data_desc &data) {}

std::pair<std::vector<data_desc>, time_point> data_for_plot(const plot_command_2d &plot)
{
  auto [row_data, timebase] = row_data_for_graphs(plot.graphs);
  auto result = std::vector<data_desc>();
  result.reserve(plot.graphs.size());
  std::ranges::copy(
      std::ranges::views::transform(
          plot.graphs,
          [&](const graph_desc_2d &g)
          {
            return std::visit(
                overload(
                    [&](const expr &expr)
                    {
                      return data_for_expression_2d(g.mark, expr, settings::samples().x,
                                                    plot.x_range);
                    },
                    [&](const csv_data &c)
                    {
                      auto &rd = *std::ranges::find_if(row_data, [&](const struct row_data &r)
                                                       { return r.filename == c.path; });
                      auto vbo = data_for_using_expressions(c.expressions, rd);
                      return data_desc(std::move(vbo), 2, rd.data.num_points);
                    },
                    [&](const parametric_data_2d &c)
                    {
                      return data_for_parametric_2d(c.expressions, settings::samples().x,
                                                    plot.t_range);
                    }),
                g.data);
          }),
      std::back_inserter(result));
  return std::make_pair(std::move(result), timebase);
}

std::vector<data_desc> data_for_plot(const plot_command_3d &plot)
{
  auto [row_data, time_base] = row_data_for_graphs(plot.graphs);
  auto result = std::vector<data_desc>();
  result.reserve(plot.graphs.size());
  std::ranges::copy(
      std::ranges::views::transform(
          plot.graphs,
          [&](const graph_desc_3d &g)
          {
            return std::visit(
                overload(
                    [&](const expr &expr)
                    {
                      return data_for_expression_3d(expr, settings::isosamples(),
                                                    settings::samples(), plot.x_range,
                                                    plot.y_range);
                    },
                    [&](const csv_data &c)
                    {
                      auto &rd = *std::ranges::find_if(
                          row_data, [&](const struct row_data &r)
                          { return r.filename == c.path && r.columns.has_value() == c.matrix; });
                      auto vbo = data_for_using_expressions(c.expressions, rd);
                      if (rd.columns.has_value() && g.mark == mark_type_3d::lines)
                      {
                        return grid_data_for_lines(std::move(vbo), rd.data.num_points, *rd.columns);
                      }
                      else
                      {
                        return data_desc(std::move(vbo), 3, rd.data.count);
                      }
                    },
                    [&](const parametric_data_3d &c)
                    {
                      return data_for_parametric_3d(c.expressions, settings::isosamples(),
                                                    settings::samples(), plot.u_range,
                                                    plot.v_range);
                    }),
                g.data);
          }),
      std::back_inserter(result));
  return result;
}

data_desc reshape(data_desc d, std::uint32_t new_point_size)
{
  if (new_point_size >= d.point_size)
  {
    assert(new_point_size % d.point_size == 0);
    const auto factor = new_point_size / d.point_size;
    assert(d.num_points % factor == 0);
    return data_desc(std::move(d.vbo), new_point_size, d.num_points / factor,
                     static_cast<uint32_t>(d.count.size()));
  }
  else
  {
    assert(d.point_size % new_point_size == 0);
    const auto factor = d.point_size / new_point_size;
    return data_desc(std::move(d.vbo), new_point_size, d.num_points * factor,
                     static_cast<uint32_t>(d.count.size()));
  }
}

} // namespace explot
