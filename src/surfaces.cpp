#include "surfaces.hpp"
#include "data.hpp"
#include "gl-handle.hpp"
#include "program.hpp"
#include <fmt/format.h>
#include "colors.hpp"
#include "settings.hpp"

namespace
{
using namespace explot;
constexpr auto vertex_shader_src = R"(#version 330 core
layout (location = 0) in vec3 position;

uniform PhaseToClip
{
  mat4 phase_to_clip;
};

uniform ClipToScreen
{
  mat4 clip_to_screen;
};

uniform ScreenToClip
{
  mat4 screen_to_clip;
};

void main()
{
  gl_Position = phase_to_clip * vec4(position, 1);
}
)";

constexpr auto fragment_shader_src = R"(#version 330 core

out vec4 FragColor;

uniform vec4 color;

void main()
{
  FragColor = color;
}
)";

constexpr auto normals_compute_shader = R"(#version 430 core

layout(local_size_x = 1, local_size_y = 1) in;

layout(std430, binding = 0) buffer block123
{
  float data[];
};

layout(std430, binding = 1) buffer block234
{
  float normals[];
};

uniform uint num_rows;
uniform uint num_columns;

uint index(uint r, uint c)
{
  return r * num_columns + c;
}

vec3 inputData(uint r, uint c)
{
  uint base = 3 * index(r,c);
  return vec3(data[base], data[base + 1], data[base + 2]);
}

void setOutput(uint r, uint c, vec3 v)
{
  uint base = 3 * index(r,c);
  normals[base ] = v.x;
  normals[base+1] = v.y;
  normals[base+2] = v.z;
}

vec3 normal(vec3 v1, vec3 v2, vec3 v3)
{
  return normalize(cross(v2 - v1, v3 - v1));
}

void main()
{
  uint row = gl_GlobalInvocationID.x;
  uint col = gl_GlobalInvocationID.y;

  vec3 n = vec3(0, 0, 0);

  if (row > 0)
  {
    if (col > 0)
    {
      n += normal(inputData(row - 1, col), inputData(row, col), inputData(row, col - 1));
    }

    if (col < num_columns - 1)
    {
      n += normal(inputData(row, col), inputData(row - 1, col), inputData(row - 1, col + 1));
      n += normal(inputData(row, col), inputData(row - 1, col + 1), inputData(row, col + 1));
    }
  }
  if (row < num_rows - 1)
  {
    if (col < num_columns - 1)
    {
      n += normal(inputData(row, col), inputData(row, col + 1), inputData(row + 1, col));
    }

    if (col > 0)
    {
      n += normal(inputData(row, col), inputData(row + 1, col), inputData(row + 1, col - 1));
      n += normal(inputData(row, col), inputData(row + 1, col - 1), inputData(row, col - 1));
    }
  }

  setOutput(row, col, normalize(n));

}
)";

void normals(gl_id vbo, const grid_data_desc &d, gl_id out)
{
  const auto num_points = d.num_columns * d.num_rows;

  glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_TRANSFORM_FEEDBACK_BARRIER_BIT);

  auto vao = make_vao();
  // auto data = std::vector<glm::vec3>(num_points, glm::vec3(0.0f, 0.0f, 1.0f));
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, out);
  glBufferData(GL_ARRAY_BUFFER, num_points * sizeof(glm::vec3), nullptr, GL_STATIC_DRAW);

  auto program = make_compute_program(normals_compute_shader);

  glUseProgram(program);
  uniform ufs[] = {{"num_columns", d.num_columns}, {"num_rows", d.num_rows}};
  set_uniforms(program, ufs);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vbo);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, out);

  glDispatchCompute(d.num_rows, d.num_columns, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // auto m = static_cast<const glm::vec3 *>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));
  // auto f = std::ofstream("normals.csv");
  // for (auto r = 0u; r < d.num_rows; ++r)
  // {
  //   for (auto c = 0u; c < d.num_columns; ++c)
  //   {
  //     f << m[r * d.num_columns + c].length() << ';';
  //   }
  //   f << '\n';
  // }
  // glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

auto program_for_surface(const explot::uniform_bindings_3d &bds)
{
  auto program = make_program(nullptr, vertex_shader_src, fragment_shader_src);

  glUseProgram(program);
  auto phase_to_clip_idx = glGetUniformBlockIndex(program, "PhaseToClip");
  glUniformBlockBinding(program, phase_to_clip_idx, bds.phase_to_clip);

  return program;
}

} // namespace

namespace explot
{
surface::surface(gl_id vbo, const grid_data_desc &d, const glm::vec4 &color,
                 const uniform_bindings_3d &bds)
    : vao(make_vao()), program(program_for_surface(bds)), data(surface_draw_info(d))
{
  auto uf = uniform{"color", color};
  set_uniforms(program, {&uf, 1});

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
}

void draw(const surface &s)
{
  glBindVertexArray(s.vao);
  glUseProgram(s.program);
  glMultiDrawElements(GL_TRIANGLE_STRIP, s.data.count.data(), GL_UNSIGNED_INT,
                      reinterpret_cast<const void *const *>(s.data.starts.data()),
                      s.data.count.size());
}

surface_lines::surface_lines(gl_id vbo, const grid_data_desc &d, const line_type &lt,
                             const uniform_bindings_3d bds)
    : offsets(make_vbo()), surface(vbo, d, background_color, bds),
      upper_lines(vbo, d, offsets, 0.002f, lt.width, lt.color, bds),
      lower_lines(vbo, d, offsets, -0.002f, settings::line_type_by_index(lt.index + 1).width,
                  settings::line_type_by_index(lt.index + 1).color, bds)
{
  normals(vbo, d, offsets);
}

void draw(const surface_lines &s)
{
  draw(s.surface);
  draw(s.upper_lines);
  draw(s.lower_lines);
}
} // namespace explot
