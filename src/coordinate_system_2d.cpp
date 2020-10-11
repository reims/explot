#include "coordinate_system_2d.hpp"
#include "data.hpp"
#include "gl-handle.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include "colors.hpp"

namespace
{
auto data_for_axes(glm::vec3 min, glm::vec3 max)
{
  auto data = std::array<float, 12>{min.x, min.y, 0.0f, max.x, min.y, 0.0f,
                                    min.x, min.y, 0.0f, min.x, max.y, 0.0f};
  return data;
}

constexpr auto ticks_vertex_shader_src = R"shader(#version 330 core
uniform mat4 phase_to_screen;
uniform vec2 axis_dir;
uniform vec2 start;
uniform float step;

void main()
{
  vec2 pos = start + (gl_VertexID * step) * axis_dir;
  gl_Position = phase_to_screen * vec4(pos, 0, 1);
}
 )shader";

constexpr auto ticks_geometry_shader_src = R"shader(#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 6) out;

uniform mat4 screen_to_clip;
uniform float tick_size;
uniform float width;
uniform vec2 tick_dir;
uniform vec2 axis_dir;

out float dist;

void main()
{
  vec2 tick_ray = (tick_size / 2.0) * tick_dir;
  vec2 width_ray = (width / 2.0) * axis_dir;
  vec2 pos = gl_in[0].gl_Position.xy;
  vec4 p1 = screen_to_clip * vec4(pos + tick_ray + width_ray, 0, 1);
  vec4 p2 = screen_to_clip * vec4(pos + tick_ray - width_ray, 0, 1);
  vec4 p3 = screen_to_clip * vec4(pos - tick_ray + width_ray, 0, 1);
  vec4 p4 = screen_to_clip * vec4(pos - tick_ray - width_ray, 0, 1);

  dist = 1;
  gl_Position = p1;
  EmitVertex();
  dist = -1;
  gl_Position = p2;
  EmitVertex();
  dist = 1;
  gl_Position = p3;
  EmitVertex();
  EndPrimitive();

  dist = 1;
  gl_Position = p3;
  EmitVertex();
  dist = -1;
  gl_Position = p4;
  EmitVertex();
  dist = -1;
  gl_Position = p2;
  EmitVertex();
  EndPrimitive();
}
)shader";

constexpr auto fragment_shader_src = R"shader(#version 330 core
out vec4 FragColor;

in float dist;

uniform vec4 color;

float ramp(float v)
{
  float d = 0.6;
  return clamp(1 - (v - d)/(1 - d), 0, 1);
}

void main()
{
  FragColor = vec4(color.xyz, ramp(abs(dist))); 
  //vec4(clamp(dist, 0, 1), clamp(-dist, 0, 1), 0.0, 1.0);
}
)shader";

auto program_for_ticks()
{
  auto program = explot::make_program();
  auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &ticks_vertex_shader_src, nullptr);
  glCompileShader(vertex_shader);
  auto geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
  glShaderSource(geometry_shader, 1, &ticks_geometry_shader_src, nullptr);
  glCompileShader(geometry_shader);
  auto fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
  glCompileShader(fragment_shader);
  glAttachShader(program, vertex_shader);
  glAttachShader(program, geometry_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(geometry_shader);
  glDeleteShader(fragment_shader);
  return program;
}

} // namespace
namespace explot
{
coordinate_system_2d make_coordinate_system_2d(const rect &bounding_rect, int num_ticks)
{
  auto d = data_for_axes(bounding_rect.lower_bounds, bounding_rect.upper_bounds);
  auto data = data_for_span(d);
  auto axis = make_lines_state_2d(data);
  const auto steps =
      (bounding_rect.upper_bounds - bounding_rect.lower_bounds) / static_cast<float>(num_ticks - 1);
  auto x_labels = make_unique_span<gl_string>(num_ticks);
  auto y_labels = make_unique_span<gl_string>(num_ticks);
  auto atlas = make_font_atlas("0123456789.,-");
  for (auto i = 0.0; i < num_ticks; ++i)
  {
    const auto p = bounding_rect.lower_bounds + static_cast<float>(i) * steps;
    x_labels[i] = gl_string(make_gl_string(atlas.value(), fmt::format("{}", p.x)));
    y_labels[i] = gl_string(make_gl_string(atlas.value(), fmt::format("{}", p.y)));
  }
  return coordinate_system_2d{.num_ticks = num_ticks,
                              .bounding_rect = bounding_rect,
                              .program_for_ticks = program_for_ticks(),
                              .vao_for_ticks = explot::make_vao(),
                              .data_for_axis = std::move(data),
                              .axis = std::move(axis),
                              .x_labels = std::move(x_labels),
                              .y_labels = std::move(y_labels)};
}
void draw(const coordinate_system_2d &cs, const glm::mat4 &view_to_screen,
          const glm::mat4 &screen_to_clip, float width, float tick_size)
{
  glBindVertexArray(cs.vao_for_ticks);
  glUseProgram(cs.program_for_ticks);
  glUniformMatrix4fv(glGetUniformLocation(cs.program_for_ticks, "phase_to_screen"), 1, GL_FALSE,
                     glm::value_ptr(view_to_screen));
  glUniformMatrix4fv(glGetUniformLocation(cs.program_for_ticks, "screen_to_clip"), 1, GL_FALSE,
                     glm::value_ptr(screen_to_clip));
  glUniform4fv(glGetUniformLocation(cs.program_for_ticks, "color"), 1, glm::value_ptr(axis_color));
  glUniform1f(glGetUniformLocation(cs.program_for_ticks, "tick_size"), 20.0f);
  glUniform1f(glGetUniformLocation(cs.program_for_ticks, "width"), 2.0f);
  glUniform2f(glGetUniformLocation(cs.program_for_ticks, "start"), cs.bounding_rect.lower_bounds.x,
              cs.bounding_rect.lower_bounds.y);
  glUniform2f(glGetUniformLocation(cs.program_for_ticks, "axis_dir"), 1.0f, 0.0f);
  glUniform2f(glGetUniformLocation(cs.program_for_ticks, "tick_dir"), 0.0f, 1.0f);
  glUniform1f(glGetUniformLocation(cs.program_for_ticks, "step"),
              (cs.bounding_rect.upper_bounds.x - cs.bounding_rect.lower_bounds.x)
                  / static_cast<float>(cs.num_ticks - 1));
  glDrawArrays(GL_POINTS, 0, cs.num_ticks);
  glUniform2f(glGetUniformLocation(cs.program_for_ticks, "axis_dir"), 0.0f, 1.0f);
  glUniform2f(glGetUniformLocation(cs.program_for_ticks, "tick_dir"), 1.0f, 0.0f);
  glUniform1f(glGetUniformLocation(cs.program_for_ticks, "step"),
              (cs.bounding_rect.upper_bounds.y - cs.bounding_rect.lower_bounds.y)
                  / static_cast<float>(cs.num_ticks - 1));
  glDrawArrays(GL_POINTS, 0, cs.num_ticks);
  draw(cs.axis, 1.0f, view_to_screen, screen_to_clip, axis_color);
  const auto steps = (cs.bounding_rect.upper_bounds - cs.bounding_rect.lower_bounds)
                     / static_cast<float>(cs.num_ticks - 1);
  for (auto i = 0.0; i < cs.num_ticks; ++i)
  {
    const auto p_x =
        cs.bounding_rect.lower_bounds + glm::vec3(static_cast<float>(i) * steps.x, 0.0f, 0.0f);
    auto o_x = view_to_screen * glm::vec4(p_x, 1.0f);
    o_x.y += -0.5 * tick_size - 1.0f;
    draw(cs.x_labels[i], screen_to_clip, o_x, text_color, 0.5f, {0.5f, 1.0f});
    const auto p_y =
        cs.bounding_rect.lower_bounds + glm::vec3(0.0f, static_cast<float>(i) * steps.y, 0.0f);
    auto o_y = view_to_screen * glm::vec4(p_y, 1.0f);
    o_y.x += -0.5 * tick_size - 1.0f;
    draw(cs.y_labels[i], screen_to_clip, o_y, text_color, 0.5f, {1.0f, 0.5f});
  }
}
} // namespace explot
