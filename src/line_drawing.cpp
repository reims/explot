#include "line_drawing.hpp"
#include "GL/glew.h"
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <memory>
#include <numbers>
#include "program.hpp"
#include "prefix_sum.hpp"

namespace
{
using namespace explot;

constexpr auto vertex_shader_src_2d = R"shader(#version 330 core
layout (location = 0) in vec2 position;

uniform mat4 phase_to_screen;

void main()
{
  gl_Position = phase_to_screen * vec4(position, 0, 1);
}
)shader";

constexpr auto dashed_vertex_shader_src_2d = R"shader(#version 330 core
layout (location = 0) in vec2 position;
layout (location = 1) in float curve_length;

uniform mat4 phase_to_screen;

out float cl;

void main()
{
  gl_Position = phase_to_screen * vec4(position, 0, 1);
  cl = curve_length;
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

static constexpr auto dashed_geometry_shader = R"foo(#version 330 core
layout (lines) in;
layout (triangle_strip, max_vertices = 30) out;

uniform float width;
uniform mat4 screen_to_clip;
uniform vec2 shape[8];

in float cl[];

out float dist;
out float curve_length;

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
  curve_length = cl[0];
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[0];
  EmitVertex();
  dist = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[1];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  curve_length = cl[0];
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[1];
  EmitVertex();
  dist = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[2];
  EmitVertex();
  EndPrimitive();


  dist = 0.0;
  curve_length = cl[0];
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[2];
  EmitVertex();
  dist = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[3];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  curve_length = cl[1];
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[4];
  EmitVertex();
  dist = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[5];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  curve_length = cl[1];
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[5];
  EmitVertex();
  dist = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[6];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[6];
  EmitVertex();
  dist = width + 0.5;
  gl_Position = rot_shape[7];
  curve_length = cl[1];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  curve_length = cl[0];
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[0];
  EmitVertex();
  dist = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[4];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  gl_Position = cp1;
  curve_length = cl[1];
  EmitVertex();
  dist  = width + 0.5;
  gl_Position = rot_shape[4];
  EmitVertex();
  dist = 0.0;
  curve_length = cl[0];
  gl_Position = cp0;
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  curve_length = cl[0];
  gl_Position = cp0;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[0];
  gl_Position = rot_shape[3];
  EmitVertex();
  dist = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[7];
  EmitVertex();
  EndPrimitive();

  dist = 0.0;
  curve_length = cl[1];
  gl_Position = cp1;
  EmitVertex();
  dist  = width + 0.5;
  curve_length = cl[1];
  gl_Position = rot_shape[7];
  EmitVertex();
  dist = 0.0;
  gl_Position = cp0;
  curve_length = cl[0];
  EmitVertex();
  EndPrimitive();
}
)foo";

static constexpr char dashed_fragment_shader_fmt[] = R"(#version 330 core
out vec4 FragColor;

in float dist;
in float curve_length;

uniform vec4 color;
uniform float width;

float segment_length = 2;

uniform uint segments[{}];
uniform uint num_segments;

float ramp(float v)
{{
  float d = 0.6;
  return clamp(1 - (v - d)/(1 - d), 0, 1);
}}

void main()
{{
  uint segment = uint(curve_length / segment_length) % num_segments;
  uint bit_idx = segment & uint(0x1F);
  uint idx = segment >> 5;
  uint mask = uint(1 << bit_idx);
  uint flag = segments[idx] & mask;
  if (flag == uint(0))
  {{
    discard;
  }}
  FragColor = vec4(color.xyz, clamp(1 - (abs(dist) - width + 0.5), 0, 1)); 
  //vec4(clamp(dist, 0, 1), clamp(-dist, 0, 1), 0.0, 1.0);
}}
)";

auto shape_for_lines(uint32_t segments)
{
  auto result = std::make_unique<float[]>(4 * segments + 4);
  auto angleDiff = std::numbers::pi_v<float> / static_cast<float>(segments);
  for (auto i = 0u; i <= segments; ++i)
  {
    auto angle = static_cast<float>(i) * angleDiff;
    result[2 * i] = std::cos(angle);
    result[2 * i + 1] = std::sin(angle);
  }
  for (auto i = 0u; i <= segments; ++i)
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

void update_curve_length(gl_id points, gl_id length, uint32_t count,
                         const glm::mat4 &phase_to_screen)
{
  static constexpr auto shader = R"(#version 430 core

layout(local_size_x = 1) in;

layout(std430, binding = 0) buffer block1
{
  vec2 points[];
};

layout(std430, binding = 1) buffer block2
{
  float l[];
};

uniform mat4 phase_to_screen;

void main()
{
  vec3 v1 = (phase_to_screen * vec4(points[gl_GlobalInvocationID.x], 0, 1)).xyz;
  vec3 v2 = (phase_to_screen * vec4(points[gl_GlobalInvocationID.x + 1], 0, 1)).xyz;
  l[gl_GlobalInvocationID.x + 1] = distance(v2, v1);
}
)";

  auto vao = make_vao();
  glBindVertexArray(vao);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, points);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, length);

  auto program = make_program();
  auto compute = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(compute, 1, &shader, nullptr);
  glCompileShader(compute);
  glAttachShader(program, compute);
  glLinkProgram(program);
  glDeleteShader(compute);

  glUseProgram(program);
  glUniformMatrix4fv(glGetUniformLocation(program, "phase_to_screen"), 1, GL_FALSE,
                     glm::value_ptr(phase_to_screen));
  glDispatchCompute(count - 1, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  prefix_sum(length, count);
}

auto program_for_dashed_line_strip_2d(uint32_t num_uints)
{
  auto dashed_fragment_shader = fmt::format(dashed_fragment_shader_fmt, num_uints);
  auto program = make_program(dashed_geometry_shader, dashed_vertex_shader_src_2d,
                              dashed_fragment_shader.c_str());
  auto s = shape_for_lines(3);
  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "shape"), 8, s.get());
  return program;
}
} // namespace

namespace explot
{
line_strip_state_2d make_line_strip_state_2d(data_desc data)
{
  line_strip_state_2d state;

  glBindVertexArray(state.vao);
  state.program = program_for_lines_2d();

  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
  state.data = std::move(data);
  return state;
}

line_strip_state_3d make_line_strip_state_3d(data_desc data)
{
  line_strip_state_3d state;

  glBindVertexArray(state.vao);
  state.program = program_for_lines_3d();

  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
  state.data = std::move(data);

  return state;
}

lines_state_2d make_lines_state_2d(data_desc d)
{
  auto state = lines_state_2d();
  state.program = program_for_lines_2d();
  glBindVertexArray(state.vao);
  glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  state.data = std::move(d);
  return state;
}

lines_state_3d make_lines_state_3d(data_desc d)
{
  auto state = lines_state_3d();
  state.program = program_for_lines_3d();
  glBindVertexArray(state.vao);
  glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ebo);
  state.data = std::move(d);
  return state;
}

void draw(const line_strip_state_2d &state, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  uniform ufs[] = {{"width", width},
                   {"phase_to_screen", phase_to_screen},
                   {"screen_to_clip", screen_to_clip},
                   {"color", color}};
  set_uniforms(state.program, ufs);
  glMultiDrawElements(GL_LINE_STRIP, state.data.count.data(), GL_UNSIGNED_INT,
                      reinterpret_cast<const void *const *>(state.data.starts.data()),
                      state.data.count.size());
}

void draw(const line_strip_state_3d &state, float width, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  uniform ufs[] = {{"width", width},
                   {"phase_to_clip", phase_to_clip},
                   {"clip_to_screen", clip_to_screen},
                   {"screen_to_clip", screen_to_clip},
                   {"color", color}};
  set_uniforms(state.program, ufs);
  glMultiDrawElements(GL_LINE_STRIP, state.data.count.data(), GL_UNSIGNED_INT,
                      reinterpret_cast<const void *const *>(state.data.starts.data()),
                      state.data.count.size());
}

void draw(const lines_state_2d &state, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  uniform ufs[] = {{"width", width},
                   {"phase_to_screen", phase_to_screen},
                   {"screen_to_clip", screen_to_clip},
                   {"color", color}};
  set_uniforms(state.program, ufs);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(state.data.num_points));
}

void draw(const lines_state_3d &state, float width, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  glBindVertexArray(state.vao);
  uniform ufs[] = {{"width", width},
                   {"phase_to_clip", phase_to_clip},
                   {"clip_to_screen", clip_to_screen},
                   {"screen_to_clip", screen_to_clip},
                   {"color", color}};
  set_uniforms(state.program, ufs);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(state.data.num_points));
}

dashed_line_strip_state_2d::dashed_line_strip_state_2d(
    data_desc d, const std::vector<std::pair<uint32_t, uint32_t>> &dash_type)
    : data(std::move(d)), curve_length(make_vbo())
{
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, curve_length);
  glBufferData(GL_ARRAY_BUFFER, data.num_indices * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, curve_length);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);

  uint32_t num_segments = 0;
  uint32_t idx_in_uint = 0;
  std::vector<uint32_t> segments(1, 0u);
  for (auto [solid, space] : dash_type)
  {
    while (solid > 0)
    {
      auto &s = segments.back();
      auto remaining = 0x20 - (num_segments & 0x1f);
      auto remaining_solid = std::min(solid, remaining);
      s |= ((1u << remaining_solid) - 1u) << (remaining);
      solid -= remaining_solid;
      num_segments += remaining_solid;
      if (remaining_solid == remaining)
      {
        segments.push_back(0u);
      }
    }
    auto remaining = 0x20 - (num_segments & 0x1f);
    if (space >= remaining)
    {
      segments.resize(segments.size() + ((space - remaining) >> 5) + 1);
    }
    num_segments += space;
  }
  program = program_for_dashed_line_strip_2d(static_cast<uint32_t>(segments.size()));
  uniform ufs[] = {{"num_segments", num_segments}, {"segments", segments}};
  set_uniforms(program, ufs);
}

void draw(const dashed_line_strip_state_2d &state, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  update_curve_length(state.data.vbo, state.curve_length, state.data.num_points, phase_to_screen);
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  uniform ufs[] = {{"width", width},
                   {"phase_to_screen", phase_to_screen},
                   {"screen_to_clip", screen_to_clip},
                   {"color", color}};
  set_uniforms(state.program, ufs);
  glMultiDrawElements(GL_LINE_STRIP, state.data.count.data(), GL_UNSIGNED_INT,
                      reinterpret_cast<const void *const *>(state.data.starts.data()),
                      state.data.count.size());
}

} // namespace explot
