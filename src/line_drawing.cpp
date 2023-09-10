#include "line_drawing.hpp"
#include "GL/glew.h"
#include <type_traits>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <fmt/format.h>
#include <memory>
#include <numbers>
#include "program.hpp"

namespace
{
using namespace explot;

constexpr auto vertex_shader_src_2d = R"shader(#version 330 core
layout (location = 0) in vec3 position;

uniform mat4 phase_to_screen;

void main()
{
  gl_Position = phase_to_screen * vec4(position, 1);
}
)shader";

constexpr auto vertex_shader_src_3d = R"shader(#version 330 core
layout (location = 0) in vec3 position;

uniform mat4 phase_to_clip;
uniform mat4 clip_to_screen;

void main()
{
  vec4 clip_pos = phase_to_clip * vec4(position, 1);
  clip_pos = clip_pos / clip_pos.w;
  gl_Position = clip_to_screen * clip_pos;
}
)shader";

constexpr auto fragment_shader_src = R"foo(
#version 330 core
out vec4 FragColor;

in float dist;

uniform vec4 color;
uniform float width;

float ramp(float v)
{
  float d = 0.6;
  return clamp(1 - (v - d)/(1 - d), 0, 1);
}

void main()
{
  FragColor = vec4(color.xyz, clamp(1 - (abs(dist) - width + 0.5), 0, 1)); 
  //vec4(clamp(dist, 0, 1), clamp(-dist, 0, 1), 0.0, 1.0);
}
)foo";

constexpr auto lines_geometry_shader_src = R"foo(#version 330 core
layout (lines) in;
layout (triangle_strip, max_vertices = 30) out;

uniform float width;
uniform mat4 screen_to_clip;
uniform vec2 shape[8];

out float dist;

#define PI 3.1415926538

void main()
{
  vec2 p0 = gl_in[0].gl_Position.xy;
  vec2 p1 = gl_in[1].gl_Position.xy;
  vec2 p01 = normalize(p1 - p0);

  float arg = -atan(p01.y, p01.x) - PI / 2;
  mat2 m = (width + 0.5) * mat2(cos(arg), -sin(arg), sin(arg), cos(arg));
  
  vec4 rot_shape[8];
  rot_shape[0] = screen_to_clip * vec4(m * shape[0] + p0, 0, 1);
  rot_shape[1] = screen_to_clip * vec4(m * shape[1] + p0, 0, 1);
  rot_shape[2] = screen_to_clip * vec4(m * shape[2] + p0, 0, 1);
  rot_shape[3] = screen_to_clip * vec4(m * shape[3] + p0, 0, 1);
  rot_shape[4] = screen_to_clip * vec4(m * shape[4] + p1, 0, 1);
  rot_shape[5] = screen_to_clip * vec4(m * shape[5] + p1, 0, 1);
  rot_shape[6] = screen_to_clip * vec4(m * shape[6] + p1, 0, 1);
  rot_shape[7] = screen_to_clip * vec4(m * shape[7] + p1, 0, 1);
  
  vec4 cp0 = screen_to_clip * vec4(p0, 0, 1);
  vec4 cp1 = screen_to_clip * vec4(p1, 0, 1);

  dist = 0.0;
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[0];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[1];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[1];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[2];
  EmitVertex();
  EndPrimitive();


  dist = 0.0;
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[2];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[3];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[4];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[5];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[5];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[6];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[6];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[7];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[0];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[4];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[4];
  EmitVertex();
  dist = 0.0;
  gl_Position = cp0;
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[3];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[7];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[7];
  EmitVertex();
  dist = 0.0;
  gl_Position = cp0;
  EmitVertex();
  EndPrimitive();
}
)foo";

auto shape_for_lines(int segments)
{
  auto result = std::make_unique<float[]>(4 * segments + 4);
  auto angleDiff = std::numbers::pi_v<float> / segments;
  for (auto i = 0; i <= segments; ++i)
  {
    auto angle = i * angleDiff;
    result[2 * i] = std::cos(angle);
    result[2 * i + 1] = std::sin(angle);
  }
  for (auto i = 0; i <= segments; ++i)
  {
    result[2 * (i + segments + 1)] = result[2 * i];
    result[2 * (i + segments + 1) + 1] = -result[2 * i + 1];
  }
  return result;
}

auto program_for_lines_2d()
{
  auto program = make_program(lines_geometry_shader_src, vertex_shader_src_2d, fragment_shader_src);
  auto s = shape_for_lines(3);
  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "shape"), 8, s.get());
  return program;
}

auto program_for_lines_3d()
{
  auto program = make_program(lines_geometry_shader_src, vertex_shader_src_3d, fragment_shader_src);
  auto s = shape_for_lines(3);
  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "shape"), 8, s.get());
  return program;
}
} // namespace

namespace explot
{
line_strip_state_2d make_line_strip_state_2d(const data_desc &data)
{
  line_strip_state_2d state;

  glBindVertexArray(state.vao);
  state.program = program_for_lines_2d();
  glBindVertexArray(state.vao);

  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);

  state.num_points_per_segment = data.num_points / data.num_segments;
  state.num_segments = data.num_segments;

  return state;
}

line_strip_state_3d make_line_strip_state_3d(const data_desc &data)
{
  line_strip_state_3d state;

  glBindVertexArray(state.vao);
  state.program = program_for_lines_3d();
  glBindVertexArray(state.vao);

  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);

  state.num_points_per_segment = data.num_points / data.num_segments;
  state.num_segments = data.num_segments;

  return state;
}

lines_state_2d make_lines_state_2d(const data_desc &d)
{
  auto state = lines_state_2d();
  state.program = program_for_lines_2d();
  state.num_points = d.num_points;
  glBindVertexArray(state.vao);
  glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  return state;
}

lines_state_3d make_lines_state_3d(const data_desc &d)
{
  auto state = lines_state_3d();
  state.program = program_for_lines_3d();
  state.num_points = d.num_points;
  glBindVertexArray(state.vao);
  glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  return state;
}

void draw(const line_strip_state_2d &state, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  auto width_loc = glGetUniformLocation(state.program, "width");
  glUniform1f(width_loc, width);
  auto phase_to_screen_loc = glGetUniformLocation(state.program, "phase_to_screen");
  glUniformMatrix4fv(phase_to_screen_loc, 1, GL_FALSE, glm::value_ptr(phase_to_screen));
  auto screen_to_clip_loc = glGetUniformLocation(state.program, "screen_to_clip");
  glUniformMatrix4fv(screen_to_clip_loc, 1, GL_FALSE, glm::value_ptr(screen_to_clip));
  auto color_loc = glGetUniformLocation(state.program, "color");
  glUniform4fv(color_loc, 1, glm::value_ptr(color));
  for (auto i = 0u; i < state.num_segments; ++i)
  {
    glDrawArrays(GL_LINE_STRIP, i * state.num_points_per_segment, state.num_points_per_segment);
  }
}

void draw(const line_strip_state_3d &state, float width, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  auto width_loc = glGetUniformLocation(state.program, "width");
  glUniform1f(width_loc, width);
  auto phase_to_clip_loc = glGetUniformLocation(state.program, "phase_to_clip");
  glUniformMatrix4fv(phase_to_clip_loc, 1, GL_FALSE, glm::value_ptr(phase_to_clip));
  auto clip_to_screen_loc = glGetUniformLocation(state.program, "clip_to_screen");
  glUniformMatrix4fv(clip_to_screen_loc, 1, GL_FALSE, glm::value_ptr(clip_to_screen));
  auto screen_to_clip_loc = glGetUniformLocation(state.program, "screen_to_clip");
  glUniformMatrix4fv(screen_to_clip_loc, 1, GL_FALSE, glm::value_ptr(screen_to_clip));
  auto color_loc = glGetUniformLocation(state.program, "color");
  glUniform4fv(color_loc, 1, glm::value_ptr(color));
  for (auto i = 0u; i < state.num_segments; ++i)
  {
    glDrawArrays(GL_LINE_STRIP, i * state.num_points_per_segment, state.num_points_per_segment);
  }
}

void draw(const lines_state_2d &state, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  auto width_loc = glGetUniformLocation(state.program, "width");
  glUniform1f(width_loc, width);
  auto phase_to_screen_loc = glGetUniformLocation(state.program, "phase_to_screen");
  glUniformMatrix4fv(phase_to_screen_loc, 1, GL_FALSE, glm::value_ptr(phase_to_screen));
  auto screen_to_clip_loc = glGetUniformLocation(state.program, "screen_to_clip");
  glUniformMatrix4fv(screen_to_clip_loc, 1, GL_FALSE, glm::value_ptr(screen_to_clip));
  auto color_loc = glGetUniformLocation(state.program, "color");
  glUniform4fv(color_loc, 1, glm::value_ptr(color));
  glDrawArrays(GL_LINES, 0, state.num_points);
}

void draw(const lines_state_3d &state, float width, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  auto width_loc = glGetUniformLocation(state.program, "width");
  glUniform1f(width_loc, width);
  auto phase_to_clip_loc = glGetUniformLocation(state.program, "phase_to_clip");
  glUniformMatrix4fv(phase_to_clip_loc, 1, GL_FALSE, glm::value_ptr(phase_to_clip));
  auto clip_to_screen_loc = glGetUniformLocation(state.program, "clip_to_screen");
  glUniformMatrix4fv(clip_to_screen_loc, 1, GL_FALSE, glm::value_ptr(clip_to_screen));
  auto screen_to_clip_loc = glGetUniformLocation(state.program, "screen_to_clip");
  glUniformMatrix4fv(screen_to_clip_loc, 1, GL_FALSE, glm::value_ptr(screen_to_clip));
  auto color_loc = glGetUniformLocation(state.program, "color");
  glUniform4fv(color_loc, 1, glm::value_ptr(color));
  glDrawArrays(GL_LINES, 0, state.num_points);
}

} // namespace explot
