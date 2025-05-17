#include "point_drawing.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "program.hpp"

namespace
{
using namespace explot;
constexpr auto vertex_2d_shader_src = R"shader(#version 330 core
layout (location = 0) in vec2 position;

uniform mat4 phase_to_screen;

void main()
{
  gl_Position = floor(phase_to_screen * vec4(position, 0, 1)) + vec4(0.5, 0.5, 0.0, 0.0);
}
)shader";

constexpr auto vertex_3d_shader_src = R"(#version 330 core
layout (location = 0) in vec3 position;

uniform mat4 phase_to_clip;
uniform mat4 clip_to_screen;

void main()
{
  vec4 clip_pos = phase_to_clip * vec4(position, 1);
  clip_pos = clip_pos / clip_pos.w;
  gl_Position = floor(clip_to_screen * clip_pos) + vec4(0.5, 0.5, 0.0, 0.0);
  gl_Position.w = 1.0;
})";

constexpr auto geometry_shader_src = R"shader(#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 12) out;

uniform float width;
uniform float point_width;
uniform mat4 screen_to_clip;

out float dist;

void main()
{
  vec2 pos = gl_in[0].gl_Position.xy;
  
  gl_Position = screen_to_clip * vec4(pos.x - width, pos.y + point_width, 0, 1);
  dist = -1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x + width, pos.y + point_width, 0, 1);
  dist = 1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x - width, pos.y - point_width, 0, 1);
  dist = -1.0;
  EmitVertex();
  EndPrimitive();

  gl_Position = screen_to_clip * vec4(pos.x + width, pos.y + point_width, 0, 1);
  dist = 1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x + width, pos.y - point_width, 0, 1);
  dist = 1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x - width, pos.y - point_width, 0, 1);
  dist = -1.0;
  EmitVertex();
  EndPrimitive();

  gl_Position = screen_to_clip * vec4(pos.x + point_width, pos.y - width, 0, 1);
  dist = -1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x + point_width, pos.y + width, 0, 1);
  dist = 1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x - point_width, pos.y - width, 0, 1);
  dist = -1.0;
  EmitVertex();
  EndPrimitive();

  gl_Position = screen_to_clip * vec4(pos.x + point_width, pos.y + width, 0, 1);
  dist = 1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x - point_width, pos.y + width, 0, 1);
  dist = 1.0;
  EmitVertex();
  gl_Position = screen_to_clip * vec4(pos.x - point_width, pos.y - width, 0, 1);
  dist = -1.0;
  EmitVertex();
  EndPrimitive();
}
)shader";

constexpr auto fragment_shader_src = R"foo(
#version 330 core
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
)foo";

program_handle make_points_program(const char *vertex_shader_src)
{
  return make_program(geometry_shader_src, vertex_shader_src, fragment_shader_src);
}

program_handle make_points_2d_program() { return make_points_program(vertex_2d_shader_src); }

program_handle make_points_3d_program() { return make_points_program(vertex_3d_shader_src); }
} // namespace

namespace explot
{
points_2d_state::points_2d_state(data_desc d)
    : vao(make_vao()), program(make_points_2d_program()), data(std::move(d))
{
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
}

void draw(const points_2d_state &state, float line_width, float point_width, const glm::vec4 &color,
          const glm::mat4 &view_to_screen, const glm::mat4 &screen_to_clip)
{
  glBindVertexArray(state.vao);
  uniform ufs[] = {{"width", line_width / 2},
                   {"point_width", point_width / 2},
                   {"screen_to_clip", screen_to_clip},
                   {"phase_to_screen", view_to_screen},
                   {"color", color}};
  set_uniforms(state.program, ufs);
  glDrawArrays(GL_POINTS, 0, static_cast<int32_t>(state.data.num_points));
}

points_3d_state::points_3d_state(data_desc d)
    : vao(make_vao()), program(make_points_3d_program()), data(std::move(d))
{
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
}

void draw(const points_3d_state &state, float line_width, float point_width, const glm::vec4 &color,
          const glm::mat4 &phase_to_clip, const glm::mat4 &clip_to_screen,
          const glm::mat4 &screen_to_clip)
{
  glBindVertexArray(state.vao);
  uniform ufs[] = {{"width", line_width / 2},          {"point_width", point_width / 2},
                   {"screen_to_clip", screen_to_clip}, {"phase_to_clip", phase_to_clip},
                   {"clip_to_screen", clip_to_screen}, {"color", color}};
  set_uniforms(state.program, ufs);
  glDrawArrays(GL_POINTS, 0, static_cast<int32_t>(state.data.num_points));
}

} // namespace explot
