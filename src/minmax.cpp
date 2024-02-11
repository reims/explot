#include "minmax.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "data.hpp"
#include <GL/glew.h>
#include "gl-handle.hpp"
#include <cstring>
#include <utility>
#include "program.hpp"

namespace
{
constexpr auto prepare_shader = R"shader(#version 330 core
layout (location = 0) in float d;
out vec2 v;

void  main()
{
  v = vec2(d, d);
}
)shader";

constexpr auto step_shader = R"shader(#version 330 core
layout (location = 0) in vec2 d1;
layout (location = 1) in vec2 d2;

out vec2 v;

void main()
{
  v = vec2(min(d1.x, d2.x), max(d1.y, d2.y));
}
)shader";

void step(unsigned int src_vbo, unsigned int tgt_vbo, int num_points)
{
  auto num_draws = static_cast<std::size_t>(num_points / 2);
  assert(num_draws > 0);
  glBindBuffer(GL_ARRAY_BUFFER, src_vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tgt_vbo, 0, 2 * num_draws * sizeof(float));
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, static_cast<int>(num_draws));
  glEndTransformFeedback();
  if (num_points & 1)
  {
    auto mapped_tgt = glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 2 * sizeof(float),
                                       GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    auto mapped_src = glMapBufferRange(GL_ARRAY_BUFFER,
                                       static_cast<std::size_t>(num_points - 1) * 2 * sizeof(float),
                                       2 * sizeof(float), GL_MAP_READ_BIT);

    glm::vec2 vec_tgt;
    glm::vec2 vec_src;
    std::memcpy(&vec_tgt, mapped_tgt, 2 * sizeof(float));
    std::memcpy(&vec_src, mapped_src, 2 * sizeof(float));
    vec_tgt.x = std::min(vec_tgt.x, vec_src.x);
    vec_tgt.y = std::max(vec_tgt.y, vec_src.y);
    std::memcpy(mapped_tgt, &vec_tgt, 2 * sizeof(float));
    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    glUnmapBuffer(GL_ARRAY_BUFFER);
  }
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0, 0, 0);
}

explot::program_handle program_for_shader(const char *shader_src)
{
  return explot::make_program_with_varying(shader_src, "v");
}

explot::vbo_handle prepare(const explot::data_desc &d, size_t stride, size_t offset)
{
  auto program = program_for_shader(prepare_shader);
  auto vao = explot::make_vao();
  glBindVertexArray(vao);
  auto vbo = explot::make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 2 * static_cast<std::size_t>(d.num_points) * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  assert(d.num_points > 0);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, 0,
                    2 * static_cast<std::size_t>(d.num_points) * sizeof(float));
  glUseProgram(program);
  glBindBuffer(GL_ARRAY_BUFFER, d.vbo);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                        (void *)(offset * sizeof(float)));
  glEnableVertexAttribArray(0);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, d.num_points);
  glEndTransformFeedback();
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0, 0, 0);
  return vbo;
}

glm::vec2 minmax(const explot::data_desc &d, size_t stride, size_t offset)
{
  auto vbo1 = prepare(d, stride, offset);
  auto vbo2 = explot::make_vbo();
  auto vao = explot::make_vao();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo2);
  glBufferData(GL_ARRAY_BUFFER, static_cast<std::size_t>(d.num_points) * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  auto num_points = d.num_points;
  auto program = program_for_shader(step_shader);
  glUseProgram(program);
  while (num_points > 1)
  {
    step(vbo1, vbo2, num_points);
    num_points >>= 1;
    std::swap(vbo1, vbo2);
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo1);
  auto b = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
  glm::vec2 result;
  std::memcpy(glm::value_ptr(result), b, 2 * sizeof(float));
  glUnmapBuffer(GL_ARRAY_BUFFER);
  return result;
}

} // namespace

namespace explot
{
glm::vec2 minmax_x(const data_desc &d, size_t stride) { return minmax(d, stride, 0); }

glm::vec2 minmax_y(const data_desc &d, size_t stride) { return minmax(d, stride, 1); }

glm::vec2 minmax_z(const data_desc &d, size_t stride) { return minmax(d, stride, 2); }

rect bounding_rect_2d(const data_desc &d, size_t stride)
{
  auto bx = minmax_x(d, stride);
  if (bx.y - bx.x < 1e-8)
  {
    bx.x -= 1.0f;
    bx.y += 1.0f;
  }
  auto by = minmax_y(d, stride);
  if (by.y - by.x < 1e-8)
  {
    by.x -= 1.0f;
    by.y += 1.0f;
  }
  // auto bz = minmax_z(d);
  return rect{.lower_bounds = glm::vec3(bx.x, by.x, -1.0f),
              .upper_bounds = glm::vec3(bx.y, by.y, 1.0f)};
}

rect bounding_rect_3d(const data_desc &d, size_t stride)
{
  auto bx = minmax_x(d, stride);
  if (bx.y - bx.x < 1e-8)
  {
    bx.x -= 1.0f;
    bx.y += 1.0f;
  }
  auto by = minmax_y(d, stride);
  if (by.y - by.x < 1e-8)
  {
    by.x -= 1.0f;
    by.y += 1.0f;
  }
  auto bz = minmax_z(d, stride);
  if (bz.y - bz.x < 1e-8)
  {
    bz.x -= 1.0f;
    bz.y += 1.0f;
  }
  return rect{.lower_bounds = glm::vec3(bx.x, by.x, bz.x),
              .upper_bounds = glm::vec3(bx.y, by.y, bz.y)};
}
} // namespace explot
