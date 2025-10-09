#include "coordinate_system_2d.hpp"
#include "data.hpp"
#include "gl-handle.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include "colors.hpp"
#include "line_drawing.hpp"
#include "program.hpp"
#include "rect.hpp"
#include "settings.hpp"

namespace
{

using namespace explot;
auto data_for_axes(glm::vec3 min, glm::vec3 max)
{
  auto data = std::array<float, 8>{min.x, min.y, max.x, min.y, min.x, min.y, min.x, max.y};
  return data;
}

constexpr auto ticks_vertex_shader_src = R"shader(#version 430 core
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

constexpr auto ticks_geometry_shader_src = R"shader(#version 430 core
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

auto make_program_for_ticks()
{
  return make_program(ticks_geometry_shader_src, ticks_vertex_shader_src, fragment_shader_src);
}

lines_state_2d make_axis(gl_id vbo, const tics_desc &tics)
{
  auto d = data_for_axes(tics.bounding_rect.lower_bounds, tics.bounding_rect.upper_bounds);
  return lines_state_2d(vbo, data_for_span(vbo, d, 2), 1.0f, axis_color);
}
} // namespace
namespace explot
{
coordinate_system_2d::coordinate_system_2d(const tics_desc &tics, uint32_t num_ticks,
                                           float tick_size, float width, time_point timebase)
    : num_ticks(num_ticks), tick_size(tick_size), bounding_rect(tics.bounding_rect),
      program_for_ticks(make_program_for_ticks()), vao_for_ticks(make_vao()),
      vbo_for_axis(make_vbo()), axis(make_axis(vbo_for_axis, tics)),
      atlas(*make_font_atlas("0123456789.,+-:abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
                             10))
{
  const auto steps = (tics.bounding_rect.upper_bounds - tics.bounding_rect.lower_bounds)
                     / static_cast<float>(num_ticks - 1);
  x_labels.reserve(num_ticks);
  y_labels.reserve(num_ticks);
  ;
  const auto use_time_base = settings::xdata() == data_type::time;
  auto timefmt = fmt::format("{{:{}}}", settings::timefmt());
  for (auto i = 0.0; i < num_ticks; ++i)
  {
    const auto p = tics.bounding_rect.lower_bounds + static_cast<float>(i) * steps;
    if (use_time_base)
    {
      using dur = time_point::duration;
      auto dt = std::chrono::duration_cast<dur>(std::chrono::duration<float>(p.x));
      auto tp = timebase + dt;
      x_labels.emplace_back(atlas, fmt::format(fmt::runtime(timefmt), tp), text_color);
    }
    else
    {
      x_labels.emplace_back(atlas, format_for_tic(p.x, tics.least_significant_digit_x), text_color);
    }
    y_labels.emplace_back(atlas, format_for_tic(p.y, tics.least_significant_digit_y), text_color);
  }
  uniform common_ufs[] = {{"color", axis_color},
                          {"tick_size", tick_size},
                          {"width", width},
                          {"start", glm::vec2(bounding_rect.lower_bounds)}};
  set_uniforms(program_for_ticks, common_ufs);
}

void update(const coordinate_system_2d &cs, const transforms_2d &t)
{
  const auto steps = (cs.bounding_rect.upper_bounds - cs.bounding_rect.lower_bounds)
                     / static_cast<float>(cs.num_ticks - 1);
  for (auto i = 0u; i < cs.num_ticks; ++i)
  {
    const auto p_x =
        cs.bounding_rect.lower_bounds + glm::vec3(static_cast<float>(i) * steps.x, 0.0f, 0.0f);
    auto o_x = t.phase_to_screen * glm::vec4(p_x, 1.0f);
    o_x.y += -2.0f * cs.tick_size - 1.0f;
    o_x = glm::floor(o_x);
    update(cs.x_labels[i], o_x, {0.5f, 1.0f}, t.screen_to_clip);
    const auto p_y =
        cs.bounding_rect.lower_bounds + glm::vec3(0.0f, static_cast<float>(i) * steps.y, 0.0f);
    auto o_y = t.phase_to_screen * glm::vec4(p_y, 1.0f);
    o_y.x += -2.0f * cs.tick_size - 1.0f;
    o_y = glm ::floor(o_y);
    update(cs.y_labels[i], o_y, {1.0f, 0.5f}, t.screen_to_clip);
  }
  update(cs.axis, t);
  glUseProgram(cs.program_for_ticks);
  glUniformMatrix4fv(glGetUniformLocation(cs.program_for_ticks, "phase_to_screen"), 1, GL_FALSE,
                     glm::value_ptr(t.phase_to_screen));
  glUniformMatrix4fv(glGetUniformLocation(cs.program_for_ticks, "screen_to_clip"), 1, GL_FALSE,
                     glm::value_ptr(t.screen_to_clip));
}

void draw(const coordinate_system_2d &cs)
{
  glBindVertexArray(cs.vao_for_ticks);
  uniform x_ufs[] = {{"axis_dir", glm::vec2(1.0f, 0.0f)},
                     {"tick_dir", glm::vec2(0.0f, 1.0f)},
                     {"step", (cs.bounding_rect.upper_bounds.x - cs.bounding_rect.lower_bounds.x)
                                  / static_cast<float>(cs.num_ticks - 1)}};
  uniform y_ufs[] = {{"axis_dir", glm::vec2(0.0f, 1.0f)},
                     {"tick_dir", glm::vec2(1.0f, 0.0f)},
                     {"step", (cs.bounding_rect.upper_bounds.y - cs.bounding_rect.lower_bounds.y)
                                  / static_cast<float>(cs.num_ticks - 1)}};
  set_uniforms(cs.program_for_ticks, x_ufs);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cs.num_ticks));
  set_uniforms(cs.program_for_ticks, y_ufs);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cs.num_ticks));
  draw(cs.axis);
  for (auto i = 0u; i < cs.num_ticks; ++i)
  {
    draw(cs.x_labels[i]);
    draw(cs.y_labels[i]);
  }
  // draw(cs.atlas, screen_to_clip, {500, 500});
}
} // namespace explot
