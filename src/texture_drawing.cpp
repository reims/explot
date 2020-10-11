#include "texture_drawing.hpp"

#include <cstdint>
#include "data.hpp"

namespace
{
static constexpr auto vertex_shader_src = R"(#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv_in;

out vec2 uv;

void main()
{
  gl_Position = vec4(pos, 0.0, 1.0);
  uv = uv_in;
}
)";

static constexpr auto fragment_shader_src = R"(#version 330 core
in vec2 uv

out vec4 FragColor;

uniform sampler2d sampler;

void main()
{
  FragColor = texture(sampler, uv);
}

)";

using namespace cpp_vis;
static constexpr float vbo_data[] = {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 0.0f,
                                     1.0f,  1.0f,  1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
                                     -1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  1.0f,  1.0f, 1.0f};
static constexpr std::uint32_t num_points = 6;
auto splat_vbo()
{
  auto d = data_for_span(vbo_data);
  return std::move(d.vbo);
}

auto splat_vao(unsigned int vbo)
{
  auto vao = make_vao();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  return vao;
}

auto splat_program()
{
  auto program = make_program();
  auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_src, nullptr);
  glCompileShader(vertex_shader);
  auto fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
  glCompileShader(fragment_shader);
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  return program;
}
} // namespace

namespace cpp_vis
{
texture_state::texture_state(unsigned int texture)
    : vbo(splat_vbo()), vao(splat_vao(vbo)), program(splat_program()), texture(texture)
{
}

void draw(const texture_state &state)
{
  glBindVertexArray(state.vao);
  glBindTexture(GL_TEXTURE_2D, state.texture);
  glUseProgram(state.program);
  glDrawArrays(GL_TRIANGLES, 0, num_points);
}

} // namespace cpp_vis
