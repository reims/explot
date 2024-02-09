#include "program.hpp"
#include "overload.hpp"
#include <glm/gtc/type_ptr.hpp>

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

program_handle make_program_with_varying(const char *shader_src, std::span<const char *> varyings)
{
  auto shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(shader, 1, &shader_src, nullptr);
  glCompileShader(shader);
  auto program = make_program();
  glAttachShader(program, shader);
  glTransformFeedbackVaryings(program, varyings.size(), varyings.data(), GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glDeleteShader(shader);
  return program;
}

program_handle make_program(const char *geometry_shader_src, const char *vertex_shader_src,
                            const char *fragment_shader_src)
{
  auto program = make_program();
  auto vertex_shader = gl_id(0);
  auto fragment_shader = gl_id(0);
  auto geometry_shader = gl_id(0);
  if (vertex_shader_src != nullptr)
  {
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_src, nullptr);
    glCompileShader(vertex_shader);
    glAttachShader(program, vertex_shader);
  }
  if (geometry_shader_src != nullptr)
  {
    geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(geometry_shader, 1, &geometry_shader_src, nullptr);
    glCompileShader(geometry_shader);
    glAttachShader(program, geometry_shader);
  }
  if (fragment_shader_src != nullptr)
  {
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
    glCompileShader(fragment_shader);
    glAttachShader(program, fragment_shader);
  }
  glLinkProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteShader(geometry_shader);
  return program;
}

void set_uniforms(gl_id program, std::span<const uniform> uniforms)
{
  glUseProgram(program);
  for (auto &uniform : uniforms)
  {
    auto loc = glGetUniformLocation(program, uniform.first);
    std::visit(overload([=](float f) { glUniform1f(loc, f); },
                        [=](const glm::vec4 &v) { glUniform4fv(loc, 1, glm::value_ptr(v)); },
                        [=](const glm::vec2 &v) { glUniform2fv(loc, 1, glm::value_ptr(v)); },
                        [=](const glm::mat4 &m)
                        { glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m)); }),
               uniform.second);
  }
}

} // namespace explot
