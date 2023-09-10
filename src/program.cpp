#include "program.hpp"
#include <optional>

namespace explot
{
program_handle make_program_with_varying(const char *shader_src, const char *varying)
{
  auto shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(shader, 1, &shader_src, nullptr);
  glCompileShader(shader);
  auto program = make_program();
  glAttachShader(program, shader);
  glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glDeleteShader(shader);
  return program;
}

program_handle make_program(const char *geometry_shader_src, const char *vertex_shader_src,
                            const char *fragment_shader_src)
{
  auto program = make_program();
  auto vertex_shader = std::optional<gl_id>();
  auto fragment_shader = std::optional<gl_id>();
  auto geometry_shader = std::optional<gl_id>();
  if (vertex_shader_src != nullptr)
  {
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(*vertex_shader, 1, &vertex_shader_src, nullptr);
    glCompileShader(*vertex_shader);
    glAttachShader(program, *vertex_shader);
  }
  if (geometry_shader_src != nullptr)
  {
    geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(*geometry_shader, 1, &geometry_shader_src, nullptr);
    glCompileShader(*geometry_shader);
    glAttachShader(program, *geometry_shader);
  }
  if (fragment_shader_src != nullptr)
  {
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(*fragment_shader, 1, &fragment_shader_src, nullptr);
    glCompileShader(*fragment_shader);
    glAttachShader(program, *fragment_shader);
  }
  glLinkProgram(program);
  if (vertex_shader)
  {
    glDeleteShader(*vertex_shader);
  }
  if (fragment_shader)
  {
    glDeleteShader(*fragment_shader);
  }
  if (geometry_shader)
  {
    glDeleteShader(*geometry_shader);
  }
  return program;
}

} // namespace explot
