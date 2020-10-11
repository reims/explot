#include "data.hpp"
#include "gl-handle.hpp"
#include <fmt/format.h>
#include "csv.hpp"
#include <ctre.hpp>
#include <charconv>
#include <array>
#include "settings.hpp"
#include "overload.hpp"
#include "range_setting.hpp"
#include <algorithm>
#include <string_view>

using namespace std::literals;

namespace
{
using namespace explot;
program_handle program_for_functional_data(std::string_view expr)
{
  static constexpr auto shader_source_fmt = R"(#version 330 core

uniform float scale;

out vec3 v;

void  main()
{{
  float x = scale * gl_InstanceID;
  float value = {};
  v = vec3(x, value, 0.0);
}}
)";

  auto shader_source = fmt::format(shader_source_fmt, expr);
  auto shader = glCreateShader(GL_VERTEX_SHADER);
  auto shader_src_ptr = shader_source.c_str();
  glShaderSource(shader, 1, &shader_src_ptr, nullptr);
  glCompileShader(shader);
  auto program = make_program();
  glAttachShader(program, shader);
  static constexpr const char *varying = "v";
  glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glDeleteShader(shader);
  return program;
}

program_handle program_for_functional_data_3d_x(std::string_view expr)
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

  const auto shader_source = fmt::format(shader_source_fmt, expr);
  auto shader = glCreateShader(GL_VERTEX_SHADER);
  auto shader_src_ptr = shader_source.c_str();
  glShaderSource(shader, 1, &shader_src_ptr, nullptr);
  glCompileShader(shader);
  auto program = make_program();
  glAttachShader(program, shader);
  static constexpr const char *varying = "v";
  glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glDeleteShader(shader);
  return program;
}

program_handle program_for_functional_data_3d_y(std::string_view expr)
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

  const auto shader_source = fmt::format(shader_source_fmt, expr);
  auto shader = glCreateShader(GL_VERTEX_SHADER);
  auto shader_src_ptr = shader_source.c_str();
  glShaderSource(shader, 1, &shader_src_ptr, nullptr);
  glCompileShader(shader);
  auto program = make_program();
  glAttachShader(program, shader);
  static constexpr const char *varying = "v";
  glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glDeleteShader(shader);
  return program;
}

static constexpr char index_regex[] = "\\$(\\d+)";
std::vector<int> extract_indices(std::string_view expression)
{
  auto result = std::vector<int>();
  while (!expression.empty())
  {
    if (auto [whole, index_str] = ctre::search<index_regex>(expression); whole)
    {
      auto index = 0;
      auto [ptr, errc] = std::from_chars(index_str.begin(), index_str.end(), index);
      result.push_back(index);
      expression = expression.substr(whole.size());
    }
    else
    {
      break;
    }
  }
  return result;
}

std::vector<int> extract_indices(std::span<const std::string> expressions)
{
  auto result = std::vector<int>();
  for (const auto &expression : expressions)
  {
    auto indices = extract_indices(expression);
    result.insert(result.end(), indices.begin(), indices.end());
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  if (!result.empty() && result.front() == 0)
  {
    result.erase(result.begin());
  }
  return result;
}

std::string replace_indices(std::string expression, const std::span<const int> indices)
{
  using namespace std::string_view_literals;
  for (auto row_index = 0u; row_index < indices.size(); ++row_index)
  {
    auto index = indices[row_index];
    auto what = fmt::format("${}", index);
    auto with = fmt::format("(row[{}])", row_index);
    auto pos = expression.find(what);
    while (pos != expression.npos)
    {
      expression.replace(pos, what.length(), with);
      pos = expression.find(what, pos + with.length());
    }
  }
  auto what = "$0"sv;
  auto with = "(gl_VertexID)"sv;
  auto pos = expression.find(what);
  while (pos != expression.npos)
  {
    expression.replace(pos, what.length(), with);
    pos = expression.find(what, pos + with.length());
  }
  return expression;
}

static constexpr const char *shader_for_2_expressions = R"shader(#version 330 core

layout(location = 0) in float row[{}];

out vec3 v;

void  main()
{{
  float x = {};
  float y = {};
  v = vec3(x, y, 0.0);
}}
)shader";

program_handle program_for_expressions(const std::array<std::string, 2> &expressions,
                                       std::span<const int> indices)
{
  auto x_expression = replace_indices(expressions[0], indices);
  auto y_expression = replace_indices(expressions[1], indices);
  auto shader_str =
      fmt::format(shader_for_2_expressions, indices.size(), x_expression, y_expression);
  auto shader_src = shader_str.c_str();
  auto shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(shader, 1, &shader_src, nullptr);
  glCompileShader(shader);
  auto program = make_program();
  glAttachShader(program, shader);
  static constexpr auto *varying = "v";
  glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glDeleteShader(shader);
  return program;
}

static constexpr const char *shader_for_3_expressions = R"shader(#version 330 core

layout(location = 0) in float row[{}];

out vec3 v;

void  main()
{{
  float x = {};
  float y = {};
  float z = {};
  v = vec3(x, y, z);
}}
)shader";

program_handle program_for_expressions(const std::array<std::string, 3> &expressions,
                                       std::span<const int> indices)
{
  auto x_expression = replace_indices(expressions[0], indices);
  auto y_expression = replace_indices(expressions[1], indices);
  auto z_expression = replace_indices(expressions[2], indices);
  auto shader_str = fmt::format(shader_for_3_expressions, indices.size(), x_expression,
                                y_expression, z_expression);
  auto shader_src = shader_str.c_str();
  auto shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(shader, 1, &shader_src, nullptr);
  glCompileShader(shader);
  auto program = make_program();
  glAttachShader(program, shader);
  static constexpr auto *varying = "v";
  glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glDeleteShader(shader);
  return program;
}

void setup_vao(GLuint vao, GLuint vbo, std::size_t num_indices)
{
  assert(num_indices > 0);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  for (auto i = 0U; i < num_indices; ++i)
  {
    glEnableVertexAttribArray(i);
    glVertexAttribPointer(i, 1, GL_FLOAT, GL_FALSE, num_indices * sizeof(float),
                          (void *)(i * sizeof(float)));
  }
}
template <std::size_t N>
data_desc data_for_expressions_helper(std::string_view path,
                                      const std::array<std::string, N> &expressions)
{
  auto vao = make_vao();
  auto indices = extract_indices(expressions);
  auto num_indices = indices.size();
  assert(num_indices > 0);
  auto data = read_csv(path, settings::datafile::separator(), indices);
  assert(data.size() % num_indices == 0);
  auto num_points = data.size() / num_indices;
  auto csv_vbo = make_vbo();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, csv_vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);
  for (auto i = 0U; i < num_indices; ++i)
  {
    glEnableVertexAttribArray(i);
    glVertexAttribPointer(i, 1, GL_FLOAT, GL_FALSE, num_indices * sizeof(float),
                          (void *)(i * sizeof(float)));
  }
  auto data_vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, data_vbo);
  glBufferData(GL_ARRAY_BUFFER, num_points, nullptr, GL_DYNAMIC_DRAW);
  auto program = program_for_expressions(expressions, indices);
  glUseProgram(program);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data_vbo);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, num_points);
  glEndTransformFeedback();
  return data_desc(std::move(data_vbo), num_points);
}

} // namespace

namespace explot
{
data_desc data_for_expressions(std::string_view path, const std::array<std::string, 2> &expressions)
{
  return data_for_expressions_helper(path, expressions);
}

data_desc data_for_expressions(std::string_view path, const std::array<std::string, 3> &expressions)
{
  return data_for_expressions_helper(path, expressions);
}

data_desc data_for_expr(std::string_view expr, std::size_t num_points)
{
  auto vao = make_vao();
  auto program = program_for_functional_data(expr);
  auto vbo = make_vbo();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 3 * num_points * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
  glUseProgram(program);
  auto loc = glGetUniformLocation(program, "scale");
  glUniform1f(loc, 1.0f);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArraysInstanced(GL_POINTS, 0, 1, num_points);
  glEndTransformFeedback();
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

  return data_desc(std::move(vbo), num_points);
}

data_desc data_for_span(std::span<const float> data)
{
  auto vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * data.size(), data.data(), GL_DYNAMIC_DRAW);
  return {std::move(vbo), static_cast<std::uint32_t>(data.size() / 3)};
}

data_desc data_for_span(std::span<const glm::vec3> data)
{
  auto vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * data.size(), data.data(), GL_DYNAMIC_DRAW);
  return {std::move(vbo), static_cast<std::uint32_t>(data.size())};
}

data_desc data_from_source(const data_source_2d &src)
{
  assert(settings::samples().x > 0);
  return std::visit(
      overload([](const std::string &expr) { return data_for_expr(expr, settings::samples().x); },
               [](const csv_data_2d &c) { return data_for_expressions(c.path, c.expressions); }),
      src);
}

data_desc data_3d_for_expression(std::string_view expr, settings::samples_setting isosamples,
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

data_desc data_from_source(const data_source_3d &src, const range_setting &x_range,
                           const range_setting &y_range)
{
  return std::visit(overload(
                        [&](const std::string &expr)
                        {
                          return data_3d_for_expression(expr, settings::isosamples(),
                                                        settings::samples(), x_range, y_range);
                        },
                        [](const csv_data_3d &c)
                        { return data_for_expressions(c.path, c.expressions); }),
                    src);
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

} // namespace explot
