#include "program.hpp"
#include "gl-handle.hpp"
#include "overload.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>

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
    GLint compile_status;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE)
    {
      fmt::println("failed to compile vertex shader");
    }
    glAttachShader(program, vertex_shader);
  }
  if (geometry_shader_src != nullptr)
  {
    geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(geometry_shader, 1, &geometry_shader_src, nullptr);
    glCompileShader(geometry_shader);
    GLint compile_status;
    glGetShaderiv(geometry_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE)
    {
      fmt::println("failed to compile geometry shader");
    }
    glAttachShader(program, geometry_shader);
  }
  if (fragment_shader_src != nullptr)
  {
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
    glCompileShader(fragment_shader);
    GLint compile_status;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status != GL_TRUE)
    {
      fmt::println("failed to compile fragment shader");
    }
    glAttachShader(program, fragment_shader);
  }
  glLinkProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteShader(geometry_shader);
  return program;
}

program_handle make_compute_program(const char *compute_shader_src)
{
  auto program = make_program();

  auto compute_shader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(compute_shader, 1, &compute_shader_src, nullptr);
  glCompileShader(compute_shader);
  glAttachShader(program, compute_shader);
  glLinkProgram(program);
  glDeleteShader(compute_shader);

  return program;
}

void set_uniforms(gl_id program, std::span<const uniform> uniforms)
{
  glUseProgram(program);
  for (auto &uniform : uniforms)
  {
    auto loc = glGetUniformLocation(program, uniform.first);
    std::visit(overload([=](float f) { glUniform1f(loc, f); },
                        [=](uint32_t i) { glUniform1ui(loc, i); }, [=](const glm::vec4 &v)
                        { glUniform4fv(loc, 1, glm::value_ptr(v)); }, [=](const glm::vec2 &v)
                        { glUniform2fv(loc, 1, glm::value_ptr(v)); }, [=](const glm::mat4 &m)
                        { glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m)); },
                        [=](std::span<const float> s) { glUniform1fv(loc, s.size(), s.data()); },
                        [=](std::span<const uint32_t> s)
                        { glUniform1uiv(loc, s.size(), s.data()); }),
               uniform.second);
  }
}

} // namespace explot
