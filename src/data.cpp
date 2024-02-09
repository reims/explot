#include "data.hpp"
#include "gl-handle.hpp"
#include <fmt/ranges.h>
#include "csv.hpp"
#include <charconv>
#include <array>
#include "settings.hpp"
#include "overload.hpp"
#include "range_setting.hpp"
#include <algorithm>
#include <ranges>
#include <string_view>
#include "program.hpp"
#include <unordered_map>

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
    std::string operator()(const box<var_or_call> &vc) const
    {
      if (!vc->params)
      {
        return vc->name;
      }
      else
      {
        const auto &ps = vc->params.value();
        return fmt::format(
            "{}({})", vc->name,
            fmt::join(views::transform(ps, [this](const expr &p) { return std::visit(*this, p); }),
                      ", "));
      }
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
  };
  return std::visit(visitor{indices}, e);
}

program_handle program_for_using_expressions(std::span<const expr> exprs,
                                             std::span<const int> indices)
{
  static constexpr char shader_source_fmt[] = R"(#version 330 core
layout(location = 0) in float row[{}];

{}

void main()
{{
{}
}}
)";

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

  auto shader_src = fmt::format(shader_source_fmt, indices.size(), varyings_str, assignments_str);

  auto program = make_program_with_varying(shader_src.c_str(), varyings_ptrs);

  return program;
}

program_handle program_for_functional_data_2d(const expr &e)
{
  static constexpr auto shader_source_fmt = R"(#version 330 core

uniform float min_x;
uniform float step_x;

out vec2 v;

void  main()
{{
  float x = min_x + step_x * gl_InstanceID;
  float value = {};
  v = vec2(x, value);
}}
)";
  auto glsl = to_glsl(e);
  auto shader_source = fmt::format(shader_source_fmt, glsl);
  return make_program_with_varying(shader_source.c_str(), "v");
}

program_handle program_for_parametric_data_2d(const expr &x_expr, const expr &y_expr)
{
  static constexpr auto shader_source_fmt = R"(#version 330 core

uniform float min_t;
uniform float step_t;

out vec2 v;

void  main()
{{
  float t = mint_t + step_t * gl_InstanceID;
  float x_value = {};
  float y_value = {};
  v = vec2(x_value, y_value);
}}
)";
  auto x_glsl = to_glsl(x_expr);
  auto y_glsl = to_glsl(y_expr);
  auto shader_source = fmt::format(shader_source_fmt, x_glsl, y_glsl);
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

void  main()
{{
  float x = min_x + floor(gl_VertexID / num_points_per_line) * step_x;
  float y = min_y + (gl_VertexID % num_points_per_line) * step_y;
  float value = {};
  v = vec3(x, y, value);
}}
)shader";
  auto glsl = to_glsl(e);
  const auto shader_source = fmt::format(shader_source_fmt, glsl);
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

void  main()
{{
  float x = min_x + (gl_VertexID % num_points_per_line) * step_x;
  float y = min_y + floor(gl_VertexID / num_points_per_line) * step_y;
  float value = {};
  v = vec3(x, y, value);
}}
)shader";
  const auto glsl = to_glsl(e);
  const auto shader_source = fmt::format(shader_source_fmt, glsl);
  return make_program_with_varying(shader_source.c_str(), "v");
}

program_handle program_for_parametric_data_3d_u(const expr &x_expr, const expr &y_expr,
                                                const expr &z_expr)
{
  static constexpr auto shader_source_fmt = R"shader(#version 330 core

uniform int num_points_per_line;
uniform float min_u;
uniform float step_u;
uniform float min_v;
uniform float step_v;

out vec3 p;

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
  const auto x_glsl = to_glsl(x_expr);
  const auto y_glsl = to_glsl(y_expr);
  const auto z_glsl = to_glsl(z_expr);
  const auto shader_source = fmt::format(shader_source_fmt, x_glsl, y_glsl, z_glsl);
  return make_program_with_varying(shader_source.c_str(), "p");
}

program_handle program_for_parametric_data_3d_v(const expr &x_expr, const expr &y_expr,
                                                const expr &z_expr)
{
  static constexpr auto shader_source_fmt = R"shader(#version 330 core

uniform int num_points_per_line;
uniform float min_u;
uniform float step_u;
uniform float min_v;
uniform float step_v;

out vec3 p;

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
  const auto x_glsl = to_glsl(x_expr);
  const auto y_glsl = to_glsl(y_expr);
  const auto z_glsl = to_glsl(z_expr);
  const auto shader_source = fmt::format(shader_source_fmt, x_glsl, y_glsl, z_glsl);
  return make_program_with_varying(shader_source.c_str(), "p");
}

void extract_indices(const expr &e, std::vector<int> &indices)
{
  struct visitor
  {
    std::vector<int> *indices;
    void operator()(const literal_expr &) {}
    void operator()(const box<var_or_call> &vc)
    {
      if (vc->params.has_value())
      {
        for (const auto &e : vc->params.value())
        {
          std::visit(*this, e);
        }
      }
    }
    void operator()(const box<unary_op> &u) { std::visit(*this, u->operand); }
    void operator()(const box<binary_op> &b)
    {
      std::visit(*this, b->lhs);
      std::visit(*this, b->rhs);
    }
    void operator()(data_ref d) { indices->push_back(d.idx); }
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

struct row_data
{
  std::string filename;
  std::vector<int> indices;
  data_desc data;
};

template <typename T>
std::vector<row_data> row_data_for_graphs_2d(const T &gs)
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
  for (auto &[f, indices] : files)
  {
    std::ranges::sort(indices);
    indices.erase(std::ranges::unique(indices).begin(), indices.end());
    auto num_indices = indices.size();
    auto data = read_csv(f, settings::datafile::separator(), indices);
    assert(data.size() % num_indices == 0);
    auto num_points = data.size() / num_indices;
    auto csv_vbo = make_vbo();
    auto vao = make_vao();
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, csv_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    result.emplace_back(std::string(f), std::move(indices),
                        data_desc(std::move(csv_vbo), num_points));
  }
  return result;
}

data_desc data_for_using_expressions(std::span<const expr> exprs, const row_data &r)
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
  glDrawArrays(GL_POINTS, 0, num_points);
  glEndTransformFeedback();
  return data_desc(std::move(data_vbo), num_points);
}

data_desc data_for_expression_2d(const expr &expr, std::size_t num_points, range_setting xrange)
{
  auto min_x = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          xrange.lower_bound.value_or(-10.0f));
  auto max_x = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          xrange.upper_bound.value_or(10.0f));
  const auto step_x = (max_x - min_x) / (num_points - 1);
  auto vao = make_vao();
  auto program = program_for_functional_data_2d(expr);
  auto vbo = make_vbo();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 2 * num_points * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
  glUseProgram(program);
  glUniform1f(glGetUniformLocation(program, "min_x"), min_x);
  glUniform1f(glGetUniformLocation(program, "step_x"), step_x);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArraysInstanced(GL_POINTS, 0, 1, num_points);
  glEndTransformFeedback();
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

  return data_desc(std::move(vbo), num_points);
}

data_desc data_for_parametric_2d(const expr &x_expr, const expr &y_expr, std::size_t num_points,
                                 range_setting trange)
{
  auto min_t = std::visit(overload([](float v) { return v; }, [](auto_scale) { return -10.0f; }),
                          trange.lower_bound.value_or(-10.0f));
  auto max_t = std::visit(overload([](float v) { return v; }, [](auto_scale) { return 10.0f; }),
                          trange.upper_bound.value_or(10.0f));
  const auto step_t = (max_t - min_t) / (num_points - 1);
  auto vao = make_vao();
  auto program = program_for_parametric_data_2d(x_expr, y_expr);
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

  return data_desc(std::move(vbo), num_points);
}

data_desc data_for_expression_3d(const expr &expr, settings::samples_setting isosamples,
                                 settings::samples_setting samples, range_setting xrange,
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
  // necessary, because data_desc only handles one segment size for all
  assert(samples.x == samples.y);
  auto program_x = program_for_functional_data_3d_x(expr);
  auto program_y = program_for_functional_data_3d_y(expr);
  glUseProgram(program_x);
  const auto line_step_x = (max_x - min_x) / (isosamples.x - 1);
  const auto point_step_x = (max_x - min_x) / (samples.x - 1);
  const auto line_step_y = (max_y - min_y) / (isosamples.y - 1);
  const auto point_step_y = (max_y - min_y) / (samples.y - 1);
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
  glUniform1f(glGetUniformLocation(program_x, "step_x"), line_step_x);
  glUniform1f(glGetUniformLocation(program_x, "min_y"), min_y);
  glUniform1f(glGetUniformLocation(program_x, "step_y"), point_step_y);
  glDrawArrays(GL_POINTS, 0, num_points_x);
  glEndTransformFeedback();
  glUseProgram(program_y);
  glUniform1i(glGetUniformLocation(program_y, "num_points_per_line"), samples.y);
  glUniform1f(glGetUniformLocation(program_y, "min_x"), min_x);
  glUniform1f(glGetUniformLocation(program_y, "step_x"), point_step_x);
  glUniform1f(glGetUniformLocation(program_y, "min_y"), min_y);
  glUniform1f(glGetUniformLocation(program_y, "step_y"), line_step_y);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, num_points_x * sizeof(glm::vec3),
                    num_points_y * sizeof(glm::vec3));
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, num_points_y);
  glEndTransformFeedback();
  return data_desc(std::move(vbo), num_points, isosamples.x + isosamples.y);
}

data_desc data_for_parametric_3d(const expr &x_expr, const expr &y_expr, const expr &z_expr,
                                 settings::samples_setting isosamples,
                                 settings::samples_setting samples, range_setting u_range,
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
  // necessary, because data_desc only handles one segment size for all
  assert(samples.x == samples.y);
  auto program_x = program_for_parametric_data_3d_u(x_expr, y_expr, z_expr);
  auto program_y = program_for_parametric_data_3d_v(x_expr, y_expr, z_expr);
  glUseProgram(program_x);
  const auto line_step_u = (max_u - min_u) / (isosamples.x - 1);
  const auto point_step_u = (max_u - min_u) / (samples.x - 1);
  const auto line_step_v = (max_v - min_v) / (isosamples.y - 1);
  const auto point_step_v = (max_v - min_v) / (samples.y - 1);
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
  glUniform1f(glGetUniformLocation(program_x, "step_u"), line_step_u);
  glUniform1f(glGetUniformLocation(program_x, "min_v"), min_v);
  glUniform1f(glGetUniformLocation(program_x, "step_v"), point_step_v);
  glDrawArrays(GL_POINTS, 0, num_points_u);
  glEndTransformFeedback();
  glUseProgram(program_y);
  glUniform1i(glGetUniformLocation(program_y, "num_points_per_line"), samples.y);
  glUniform1f(glGetUniformLocation(program_y, "min_u"), min_u);
  glUniform1f(glGetUniformLocation(program_y, "step_u"), point_step_u);
  glUniform1f(glGetUniformLocation(program_y, "min_v"), min_v);
  glUniform1f(glGetUniformLocation(program_y, "step_v"), line_step_v);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, num_points_u * sizeof(glm::vec3),
                    num_points_v * sizeof(glm::vec3));
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, num_points_v);
  glEndTransformFeedback();
  return data_desc(std::move(vbo), num_points, isosamples.x + isosamples.y);
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
  return {std::move(vbo), static_cast<std::uint32_t>(data.size() / stride)};
}

data_desc data_for_span(std::span<const glm::vec3> data)
{
  auto vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), data.data(), GL_DYNAMIC_DRAW);
  return {std::move(vbo), static_cast<std::uint32_t>(data.size())};
}

data_desc::data_desc(vbo_handle vbo, std::uint32_t num_points, std::uint32_t num_segments)
    : vbo(std::move(vbo)), num_points(num_points), num_segments(num_segments)
{
  assert(num_points % num_segments == 0);
}

void print_data(const data_desc &data)
{
  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  auto vs = static_cast<const glm::vec3 *>(glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY));
  auto points_per_segment = data.num_points / data.num_segments;
  for (auto i = 0u; i < data.num_segments; ++i)
  {
    for (auto j = 0u; j < points_per_segment; ++j)
    {
      const auto &v = vs[i * points_per_segment + j];
      fmt::print("({}, {}, {}) ", v.x, v.y, v.z);
    }
    fmt::print("\n");
  }
  glUnmapBuffer(GL_ARRAY_BUFFER);
}

std::vector<data_desc> data_for_plot(const plot_command_2d &plot)
{
  auto row_data = row_data_for_graphs_2d(plot.graphs);
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
                    { return data_for_expression_2d(expr, settings::samples().x, plot.x_range); },
                    [&](const csv_data &c)
                    {
                      return data_for_using_expressions(
                          c.expressions,
                          *std::ranges::find_if(row_data, [&](const struct row_data &r)
                                                { return r.filename == c.path; }));
                    },
                    [&](const parametric_data_2d &c)
                    {
                      return data_for_parametric_2d(c.x_expression, c.y_expression,
                                                    settings::samples().x, plot.t_range);
                    }),
                g.data);
          }),
      std::back_inserter(result));
  return result;
}

std::vector<data_desc> data_for_plot(const plot_command_3d &plot)
{
  auto row_data = row_data_for_graphs_2d(plot.graphs);
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
                      return data_for_using_expressions(
                          c.expressions,
                          *std::ranges::find_if(row_data, [&](const struct row_data &r)
                                                { return r.filename == c.path; }));
                    },
                    [&](const parametric_data_3d &c)
                    {
                      return data_for_parametric_3d(c.x_expression, c.y_expression, c.z_expression,
                                                    settings::isosamples(), settings::samples(),
                                                    plot.u_range, plot.v_range);
                    }),
                g.data);
          }),
      std::back_inserter(result));
  return result;
}

} // namespace explot
