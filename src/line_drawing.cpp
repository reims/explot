#include "line_drawing.hpp"
#include "GL/glew.h"
#include <cstdint>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>
#include <memory>
#include <numbers>
#include "data.hpp"
#include "gl-handle.hpp"
#include "program.hpp"
#include "prefix_sum.hpp"

namespace
{
using namespace explot;

constexpr auto vertex_shader_src_2d = R"shader(#version 330 core
layout (location = 0) in vec2 position;

uniform PhaseToScreen
{
  mat4 phase_to_screen;
};


void main()
{
  gl_Position = phase_to_screen * vec4(position, 0, 1);
}
)shader";

constexpr auto dashed_vertex_shader_src_2d = R"shader(#version 330 core
layout (location = 0) in vec2 position;
layout (location = 1) in float curve_length;

uniform PhaseToScreen
{
  mat4 phase_to_screen;
};

out float cl;

void main()
{
  gl_Position = phase_to_screen * vec4(position, 0, 1);
  cl = curve_length;
}
)shader";

constexpr auto vertex_shader_src_3d = R"shader(#version 330 core
layout (location = 0) in vec3 position;

uniform PhaseToClip
{
  mat4 phase_to_clip;
};
uniform ClipToScreen
{
  mat4 clip_to_screen;
};

void main()
{
  vec4 clip_pos = phase_to_clip * vec4(position, 1);
  clip_pos = clip_pos / clip_pos.w;
  gl_Position = clip_to_screen * clip_pos;
}
)shader";

constexpr auto offset_vertex_shader_src_3d = R"shader(#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 offset;

uniform float factor;

uniform PhaseToClip
{
  mat4 phase_to_clip;
};
uniform ClipToScreen
{
  mat4 clip_to_screen;
};

void main()
{
  vec4 clip_pos = phase_to_clip * vec4(position + factor * offset, 1);
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

uniform ScreenToClip
{
  mat4 screen_to_clip;
};
uniform vec2 shape[8];

out float dist;

#define PI 3.1415926538

void main()
{
  vec2 p0 = gl_in[0].gl_Position.xy;
  vec2 p1 = gl_in[1].gl_Position.xy;
  float z0 = gl_in[0].gl_Position.z;
  float z1 = gl_in[1].gl_Position.z;
  vec2 p01 = normalize(p1 - p0);

  float arg = -atan(p01.y, p01.x) - PI / 2;
  mat2 m = (width + 0.5) * mat2(cos(arg), -sin(arg), sin(arg), cos(arg));
  
  vec4 rot_shape[8];
  rot_shape[0] = screen_to_clip * vec4(m * shape[0] + p0, z0, 1);
  rot_shape[1] = screen_to_clip * vec4(m * shape[1] + p0, z0, 1);
  rot_shape[2] = screen_to_clip * vec4(m * shape[2] + p0, z0, 1);
  rot_shape[3] = screen_to_clip * vec4(m * shape[3] + p0, z0, 1);
  rot_shape[4] = screen_to_clip * vec4(m * shape[4] + p1, z1, 1);
  rot_shape[5] = screen_to_clip * vec4(m * shape[5] + p1, z1, 1);
  rot_shape[6] = screen_to_clip * vec4(m * shape[6] + p1, z1, 1);
  rot_shape[7] = screen_to_clip * vec4(m * shape[7] + p1, z1, 1);
  
  vec4 cp0 = screen_to_clip * vec4(p0, z0, 1);
  vec4 cp1 = screen_to_clip * vec4(p1, z1, 1);

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
uniform vec2 shape[8];

uniform ScreenToClip
{
  mat4 screen_to_clip;
};

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

auto program_for_lines_2d(const uniform_bindings_2d &bds = {})
{
  auto program = make_program(lines_geometry_shader_src, vertex_shader_src_2d, fragment_shader_src);
  auto phase_to_screen_idx = glGetUniformBlockIndex(program, "PhaseToScreen");
  glUniformBlockBinding(program, phase_to_screen_idx, bds.phase_to_screen);
  auto screen_to_clip_idx = glGetUniformBlockIndex(program, "ScreenToClip");
  glUniformBlockBinding(program, screen_to_clip_idx, bds.screen_to_clip);
  auto s = shape_for_lines(3);
  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "shape"), 8, s.get());
  return program;
}

auto program_for_lines_3d(const uniform_bindings_3d &bds = {})
{
  auto program = make_program(lines_geometry_shader_src, vertex_shader_src_3d, fragment_shader_src);
  auto s = shape_for_lines(3);
  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "shape"), 8, s.get());
  auto phase_to_clip_idx = glGetUniformBlockIndex(program, "PhaseToClip");
  glUniformBlockBinding(program, phase_to_clip_idx, bds.phase_to_clip);
  auto screen_to_clip_idx = glGetUniformBlockIndex(program, "ScreenToClip");
  glUniformBlockBinding(program, screen_to_clip_idx, bds.screen_to_clip);
  auto clip_to_screen_idx = glGetUniformBlockIndex(program, "ClipToScreen");
  glUniformBlockBinding(program, clip_to_screen_idx, bds.clip_to_screen);

  return program;
}

auto program_for_offset_lines_3d(const uniform_bindings_3d &bds = {})
{
  auto program =
      make_program(lines_geometry_shader_src, offset_vertex_shader_src_3d, fragment_shader_src);
  auto s = shape_for_lines(3);
  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "shape"), 8, s.get());
  auto phase_to_clip_idx = glGetUniformBlockIndex(program, "PhaseToClip");
  glUniformBlockBinding(program, phase_to_clip_idx, bds.phase_to_clip);
  auto screen_to_clip_idx = glGetUniformBlockIndex(program, "ScreenToClip");
  glUniformBlockBinding(program, screen_to_clip_idx, bds.screen_to_clip);
  auto clip_to_screen_idx = glGetUniformBlockIndex(program, "ClipToScreen");
  glUniformBlockBinding(program, clip_to_screen_idx, bds.clip_to_screen);

  return program;
}

void update_curve_length(gl_id points, gl_id length, uint32_t count)
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

uniform PhaseToScreen
{
  mat4 phase_to_screen;
};

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

  auto phase_to_screen_idx = glGetUniformBlockIndex(program, "PhaseToScreen");
  glUniformBlockBinding(program, phase_to_screen_idx, uniform_bindings_2d().phase_to_screen);

  glUseProgram(program);
  glDispatchCompute(count - 1, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  prefix_sum(length, count);
}

auto program_for_dashed_line_strip_2d(uint32_t num_uints, const uniform_bindings_2d &bds = {})
{
  auto dashed_fragment_shader = fmt::format(dashed_fragment_shader_fmt, num_uints);
  auto program = make_program(dashed_geometry_shader, dashed_vertex_shader_src_2d,
                              dashed_fragment_shader.c_str());
  GLint link_status;
  glGetProgramiv(program, GL_LINK_STATUS, &link_status);
  if (link_status != GL_TRUE)
  {
    fmt::println("failed to link dashed line program");
  }
  auto s = shape_for_lines(3);
  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "shape"), 8, s.get());
  auto phase_to_screen_idx = glGetUniformBlockIndex(program, "PhaseToScreen");
  glUniformBlockBinding(program, phase_to_screen_idx, bds.phase_to_screen);
  auto screen_to_clip_idx = glGetUniformBlockIndex(program, "ScreenToClip");
  glUniformBlockBinding(program, screen_to_clip_idx, bds.screen_to_clip);

  return program;
}
} // namespace

namespace explot
{
line_strip_state_2d::line_strip_state_2d(gl_id vbo, const seq_data_desc &d, float width,
                                         const glm::vec4 &color, const uniform_bindings_2d &bds)
    : vao(make_vao()), program(program_for_lines_2d(bds)), data(sequential_draw_info(d))
{
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);

  uniform ufs[] = {{"width", width}, {"color", color}};
  set_uniforms(program, ufs);
}

line_strip_state_3d::line_strip_state_3d(gl_id vbo, const seq_data_desc &d, float width,
                                         const glm::vec4 &color, const uniform_bindings_3d &bds)
    : vao(make_vao()), program(program_for_lines_3d(bds)), data(sequential_draw_info(d))
{

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);

  uniform ufs[] = {{"width", width}, {"color", color}};
  set_uniforms(program, ufs);
}

line_strip_state_3d::line_strip_state_3d(gl_id vbo, const grid_data_desc &d, gl_id offsets,
                                         float factor, float width, const glm::vec4 &color,
                                         const uniform_bindings_3d &bds)
    : vao(make_vao()), program(program_for_offset_lines_3d(bds)), data(grid_lines_draw_info(d))
{
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, offsets);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
  uniform ufs[] = {{"width", width}, {"color", color}, {"factor", factor}};
  set_uniforms(program, ufs);
}

line_strip_state_3d::line_strip_state_3d(gl_id vbo, const grid_data_desc &d, float width,
                                         const glm::vec4 &color, const uniform_bindings_3d &bds)
    : vao(make_vao()), program(program_for_lines_3d(bds)), data(grid_lines_draw_info(d))
{

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);

  uniform ufs[] = {{"width", width}, {"color", color}};
  set_uniforms(program, ufs);
}

lines_state_2d::lines_state_2d(gl_id vbo, const seq_data_desc &d, float width,
                               const glm::vec4 &color, const uniform_bindings_2d &bds)
    : vao(make_vao()), program(program_for_lines_2d(bds)), data(sequential_draw_info(d))
{
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
  uniform ufs[] = {{"width", width}, {"color", color}};
  set_uniforms(program, ufs);
}

lines_state_3d::lines_state_3d(gl_id vbo, const seq_data_desc &d, float width,
                               const glm::vec4 &color, const uniform_bindings_3d &bds)
    : vao(make_vao()), program(program_for_lines_3d(bds)), data(sequential_draw_info(d))
{
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
  uniform ufs[] = {{"width", width}, {"color", color}};
  set_uniforms(program, ufs);
}

void draw(const line_strip_state_2d &state)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  glMultiDrawElements(GL_LINE_STRIP, state.data.count.data(), GL_UNSIGNED_INT,
                      reinterpret_cast<const void *const *>(state.data.starts.data()),
                      state.data.count.size());
}

void draw(const line_strip_state_3d &state)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  glMultiDrawElements(GL_LINE_STRIP, state.data.count.data(), GL_UNSIGNED_INT,
                      reinterpret_cast<const void *const *>(state.data.starts.data()),
                      state.data.count.size());
}

void draw(const lines_state_2d &state)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(state.data.num_indices));
}

void draw(const lines_state_3d &state)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(state.data.num_indices));
}

dashed_line_strip_state_2d::dashed_line_strip_state_2d(
    gl_id vbo, const seq_data_desc &d, const std::vector<std::pair<uint32_t, uint32_t>> &dash_type,
    float width, const glm::vec4 &color, const uniform_bindings_2d &bds)
    : vbo(vbo), vao(make_vao()), data(sequential_draw_info(d)), curve_length(make_vbo())
{
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, curve_length);
  glBufferData(GL_ARRAY_BUFFER, data.num_indices * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, curve_length);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);

  uint32_t num_segments = 0;
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
  program = program_for_dashed_line_strip_2d(static_cast<uint32_t>(segments.size()), bds);
  uniform ufs[] = {{"num_segments", num_segments},
                   {"segments", segments},
                   {"width", width},
                   {"color", color}};
  set_uniforms(program, ufs);
}

void update(const dashed_line_strip_state_2d &state)
{
  update_curve_length(state.vbo, state.curve_length, state.data.num_indices);
}

void draw(const dashed_line_strip_state_2d &state)
{
  glBindVertexArray(state.vao);
  glUseProgram(state.program);
  glMultiDrawElements(GL_LINE_STRIP, state.data.count.data(), GL_UNSIGNED_INT,
                      reinterpret_cast<const void *const *>(state.data.starts.data()),
                      state.data.count.size());
}

} // namespace explot
