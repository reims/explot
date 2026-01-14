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
#include <array>
namespace
{

using namespace explot;
auto data_for_axes(glm::vec3 min, glm::vec3 max)
{
  auto data = std::array<float, 8>{min.x, min.y, max.x, min.y, min.x, min.y, min.x, max.y};
  return data;
}

constexpr auto tics_vertex_shader_src = R"shader(#version 430 core
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

constexpr auto tics_geometry_shader_src = R"shader(#version 430 core
layout (points) in;
layout (triangle_strip, max_vertices = 6) out;

uniform mat4 screen_to_clip;

uniform float tic_size;
uniform float width;
uniform vec2 tic_dir;
uniform vec2 axis_dir;

out float dist;

void main()
{
  vec2 tic_ray = (tic_size / 2.0) * tic_dir;
  vec2 width_ray = (width / 2.0) * axis_dir;
  vec2 pos = gl_in[0].gl_Position.xy;
  vec4 p1 = screen_to_clip * vec4(pos + tic_ray + width_ray, 0, 1);
  vec4 p2 = screen_to_clip * vec4(pos + tic_ray - width_ray, 0, 1);
  vec4 p3 = screen_to_clip * vec4(pos - tic_ray + width_ray, 0, 1);
  vec4 p4 = screen_to_clip * vec4(pos - tic_ray - width_ray, 0, 1);

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

auto program_for_tics()
{
  return make_program(tics_geometry_shader_src, tics_vertex_shader_src, fragment_shader_src);
}

lines_state_2d make_axis(gl_id vbo)
{
  return lines_state_2d(vbo, seq_data_desc(2, 4), 1.0f, axis_color);
}
} // namespace
namespace explot
{
coordinate_system_2d::coordinate_system_2d(uint32_t num_tics, float tic_size, float width,
                                           time_point timebase, data_type xdata,
                                           std::string timefmt)
    : num_tics(num_tics), tic_size(tic_size), program_for_x_tics(program_for_tics()),
      program_for_y_tics(program_for_tics()), vao_for_tics(make_vao()), vbo_for_axis(make_vbo()),
      axis(make_axis(vbo_for_axis)), x_labels(num_tics), y_labels(num_tics),
      atlas(make_font_atlas("0123456789.,+-:abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")),
      timebase(timebase), xdata(xdata), timefmt(std::move(timefmt))
{
  assert(num_tics > 1);
  uniform common_ufs[] = {{"color", axis_color}, {"tic_size", tic_size}, {"width", width}};
  set_uniforms(program_for_x_tics, common_ufs);
  set_uniforms(program_for_y_tics, common_ufs);
  uniform x_ufs[] = {{"axis_dir", glm::vec2(1.0f, 0.0f)}, {"tic_dir", glm::vec2(0.0f, 1.0f)}};
  set_uniforms(program_for_x_tics, x_ufs);
  uniform y_ufs[] = {{"axis_dir", glm::vec2(0.0f, 1.0f)}, {"tic_dir", glm::vec2(1.0f, 0.0f)}};
  set_uniforms(program_for_y_tics, y_ufs);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_for_axis);
  glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
}

void update_view(coordinate_system_2d &cs, const tics_desc &view, const transforms_2d &t)
{
  const auto steps = (view.bounding_rect.upper_bounds - view.bounding_rect.lower_bounds)
                     / static_cast<float>(cs.num_tics - 1);

  const auto use_time_base = cs.xdata == data_type::time;
  auto timefmtstr = fmt::format("{{:{}}}", cs.timefmt);
  for (auto i = 0u; i < cs.num_tics; ++i)
  {
    const auto p = view.bounding_rect.lower_bounds + static_cast<float>(i) * steps;
    if (use_time_base)
    {
      using dur = time_point::duration;
      auto dt = std::chrono::duration_cast<dur>(std::chrono::duration<float>(p.x));
      auto tp = cs.timebase + dt;
      update(cs.x_labels[i], fmt::format(fmt::runtime(cs.timefmt), tp), cs.atlas, text_color);
    }
    else
    {
      update(cs.x_labels[i], format_for_tic(p.x, view.least_significant_digit_x), cs.atlas,
             text_color);
    }
    update(cs.y_labels[i], format_for_tic(p.y, view.least_significant_digit_y), cs.atlas,
           text_color);
  }
  uniform common_ufs[] = {{"phase_to_screen", glm::identity<glm::mat4>()},
                          {"screen_to_clip", t.screen_to_clip}};
  set_uniforms(cs.program_for_x_tics, common_ufs);
  set_uniforms(cs.program_for_y_tics, common_ufs);
  update(cs.axis,
         {.phase_to_screen = glm::identity<glm::mat4>(), .screen_to_clip = t.screen_to_clip});
}

void update_screen(coordinate_system_2d &cs, const rect &plot_screen, const transforms_2d &t)
{
  const auto steps =
      (plot_screen.upper_bounds - plot_screen.lower_bounds) / static_cast<float>(cs.num_tics - 1);
  for (auto i = 0u; i < cs.num_tics; ++i)
  {
    const auto p_x =
        plot_screen.lower_bounds + glm::vec3(static_cast<float>(i) * steps.x, 0.0f, 0.0f);
    auto o_x = glm::vec4(p_x, 1.0f);
    o_x.y += -2.0f * cs.tic_size - 1.0f;
    o_x = glm::floor(o_x);
    update(cs.x_labels[i], o_x, {0.5f, 1.0f}, t.screen_to_clip);
    const auto p_y =
        plot_screen.lower_bounds + glm::vec3(0.0f, static_cast<float>(i) * steps.y, 0.0f);
    auto o_y = glm::vec4(p_y, 1.0f);
    o_y.x += -2.0f * cs.tic_size - 1.0f;
    o_y = glm ::floor(o_y);
    update(cs.y_labels[i], o_y, {1.0f, 0.5f}, t.screen_to_clip);
  }
  update(cs.axis,
         {.phase_to_screen = glm::identity<glm::mat4>(), .screen_to_clip = t.screen_to_clip});
  uniform common_ufs[] = {{"phase_to_screen", glm::identity<glm::mat4>()},
                          {"screen_to_clip", t.screen_to_clip},
                          {"start", glm::vec2(plot_screen.lower_bounds)}};
  set_uniforms(cs.program_for_x_tics, common_ufs);
  set_uniforms(cs.program_for_y_tics, common_ufs);
  auto step_x = uniform("step", steps.x);
  set_uniforms(cs.program_for_x_tics, {&step_x, 1});
  auto step_y = uniform("step", steps.y);
  set_uniforms(cs.program_for_y_tics, {&step_y, 1});
  auto data = data_for_axes(plot_screen.lower_bounds, plot_screen.upper_bounds);
  glBindBuffer(GL_ARRAY_BUFFER, cs.vbo_for_axis);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data.data());
}

void draw(const coordinate_system_2d &cs)
{
  glBindVertexArray(cs.vao_for_tics);
  glUseProgram(cs.program_for_x_tics);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cs.num_tics));
  glUseProgram(cs.program_for_y_tics);
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cs.num_tics));

  draw(cs.axis);
  for (auto i = 0u; i < cs.num_tics; ++i)
  {
    draw(cs.x_labels[i]);
    draw(cs.y_labels[i]);
  }
}
} // namespace explot
