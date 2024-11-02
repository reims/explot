#include "coordinate_system_2d.hpp"
#include "data.hpp"
#include "gl-handle.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include "colors.hpp"
#include "program.hpp"
#include "settings.hpp"

namespace
{
auto data_for_axes(glm::vec3 min, glm::vec3 max)
{
  auto data = std::array<float, 12>{min.x, min.y, max.x, min.y, min.x, min.y, min.x, max.y};
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
  return explot::make_program(ticks_geometry_shader_src, ticks_vertex_shader_src,
                              fragment_shader_src);
}

} // namespace
namespace explot
{
coordinate_system_2d make_coordinate_system_2d(const rect &bounding_rect, int num_ticks,
                                               time_point timebase)
{
  auto d = data_for_axes(bounding_rect.lower_bounds, bounding_rect.upper_bounds);
  auto data = data_for_span(d, 2);
  auto axis = make_lines_state_2d(std::move(data));
  const auto steps =
      (bounding_rect.upper_bounds - bounding_rect.lower_bounds) / static_cast<float>(num_ticks - 1);
  auto x_labels = make_unique_span<gl_string>(num_ticks);
  auto y_labels = make_unique_span<gl_string>(num_ticks);
  auto atlas = make_font_atlas("0123456789.,-");
  const auto use_time_base = settings::xdata() == settings::data_type::time;
  auto timefmt = fmt::format("{{{}}}", settings::timefmt());
  for (auto i = 0.0; i < num_ticks; ++i)
  {
    const auto p = bounding_rect.lower_bounds + static_cast<float>(i) * steps;
    if (use_time_base)
    {
      auto tp = timebase + std::chrono::duration<float>(p.x);
      x_labels[i] =
          gl_string(make_gl_string(atlas.value(), fmt::format(fmt::runtime(timefmt), tp)));
    }
    else
    {
      x_labels[i] = gl_string(make_gl_string(atlas.value(), fmt::format("{}", p.x)));
    }
    y_labels[i] = gl_string(make_gl_string(atlas.value(), fmt::format("{}", p.y)));
  }
  return coordinate_system_2d{.num_ticks = num_ticks,
                              .bounding_rect = bounding_rect,
                              .program_for_ticks = program_for_ticks(),
                              .vao_for_ticks = explot::make_vao(),
                              .axis = std::move(axis),
                              .x_labels = std::move(x_labels),
                              .y_labels = std::move(y_labels)};
}
void draw(const coordinate_system_2d &cs, const glm::mat4 &view_to_screen,
          const glm::mat4 &screen_to_clip, float width, float tick_size)
{
  glBindVertexArray(cs.vao_for_ticks);
  uniform common_ufs[] = {{"phase_to_screen", view_to_screen},
                          {"screen_to_clip", screen_to_clip},
                          {"color", axis_color},
                          {"tick_size", 20.0f},
                          {"width", 2.0f},
                          {"start", glm::vec2(cs.bounding_rect.lower_bounds)}};
  uniform x_ufs[] = {{"axis_dir", glm::vec2(1.0f, 0.0f)},
                     {"tick_dir", glm::vec2(0.0f, 1.0f)},
                     {"step", (cs.bounding_rect.upper_bounds.x - cs.bounding_rect.lower_bounds.x)
                                  / static_cast<float>(cs.num_ticks - 1)}};
  uniform y_ufs[] = {{"axis_dir", glm::vec2(0.0f, 1.0f)},
                     {"tick_dir", glm::vec2(1.0f, 0.0f)},
                     {"step", (cs.bounding_rect.upper_bounds.y - cs.bounding_rect.lower_bounds.y)
                                  / static_cast<float>(cs.num_ticks - 1)}};
  set_uniforms(cs.program_for_ticks, common_ufs);
  set_uniforms(cs.program_for_ticks, x_ufs);
  glDrawArrays(GL_POINTS, 0, cs.num_ticks);
  set_uniforms(cs.program_for_ticks, y_ufs);
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
