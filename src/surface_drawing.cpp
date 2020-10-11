#include "surface_drawing.hpp"

#include <vector>
#include <glm/gtc/type_ptr.hpp>

namespace
{
constexpr auto vertex_shader_src = R"shader(#version 330 core
layout (location = 0) in vec3 pos;

uniform mat4 phase_to_view;
uniform mat4 view_to_clip;

void main()
{
  gl_Position = view_to_clip * phase_to_view * vec4(pos, 1.0);
}
)shader";

constexpr auto fragment_shader_src = R"shader(#version 330 core
uniform vec4 color;

out vec4 frag_color;

void main()
{
  frag_color = color;
}
)shader";

explot::program_handle make_program_for_surface()
{
  auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &fragment_shader_src, nullptr);
  glCompileShader(vertex_shader);

  auto fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
  glCompileShader(fragment_shader);

  auto program = explot::make_program();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

explot::vbo_handle indices_for_surface(std::uint32_t num_rows, std::uint32_t num_columns)
{
  auto vbo = explot::make_vbo();
  auto index = [&](std::uint32_t column, std::uint32_t row) { return column + row * num_columns; };
  auto indices = std::vector<std::uint32_t>();
  indices.reserve(2 * num_columns * (num_rows - 1));
  for (std::uint32_t row = 0; row < num_rows - 1; ++row)
  {
    for (std::uint32_t column = 0; column < num_columns; ++column)
    {
      auto actual_column = column % 2 ? column : (num_columns - column - 1);
      indices.push_back(index(row, actual_column));
      indices.push_back(index(row + 1, actual_column));
    }
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(),
               GL_STATIC_DRAW);
  return vbo;
}
} // namespace

namespace explot
{
void draw(const surface_state &surface, const glm::mat4 &phase_to_view,
          const glm::mat4 &view_to_clip)
{
  glBindVertexArray(surface.vao);
  glUseProgram(surface.program);
  glUniformMatrix4fv(surface.program, glGetUniformLocation(surface.program, "phase_to_view"),
                     GL_FALSE, glm::value_ptr(phase_to_view));
  glUniformMatrix4fv(surface.program, glGetUniformLocation(surface.program, "view_to_clip"),
                     GL_FALSE, glm::value_ptr(view_to_clip));
  glDrawElements(GL_TRIANGLE_STRIP, 1, GL_UNSIGNED_INT, nullptr);
}

surface_state::surface_state(std::shared_ptr<data_desc> data)
    : data(std::move(data)),
      indices(indices_for_surface(data->num_points / data->num_columns, data->num_columns)),
      vao(make_vao()), program(make_program_for_surface())
{
  glBindVertexArray(vao);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
  glBindBuffer(GL_ARRAY_BUFFER, data->vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
}

} // namespace explot
