#include "minmax.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>
#include "gl-handle.hpp"
#include <cstring>
#include <utility>
#include "program.hpp"

namespace
{
using namespace explot;
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

void step(gl_id src_vbo, gl_id tgt_vbo, uint32_t num_points)
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

program_handle program_for_shader(const char *shader_src)
{
  return make_program_with_varying(shader_src, "v");
}

vbo_handle prepare(gl_id dvbo, uint32_t num_points, uint32_t point_size, uint32_t offset)
{
  auto program = program_for_shader(prepare_shader);
  auto vao = make_vao();
  glBindVertexArray(vao);
  auto vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 2 * static_cast<std::size_t>(num_points) * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  assert(num_points > 0);
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo, 0,
                    2 * static_cast<std::size_t>(num_points) * sizeof(float));
  glUseProgram(program);
  glBindBuffer(GL_ARRAY_BUFFER, dvbo);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, point_size * sizeof(float),
                        (void *)(offset * sizeof(float)));
  glEnableVertexAttribArray(0);
  glBeginTransformFeedback(GL_POINTS);
  glDrawArrays(GL_POINTS, 0, static_cast<int32_t>(num_points));
  glEndTransformFeedback();
  glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0, 0, 0);
  return vbo;
}

glm::vec2 minmax(gl_id dvbo, uint32_t num_points, uint32_t point_size, uint32_t offset)
{
  if (num_points == 0)
  {
    return glm::vec2(-1.0f, 1.0f);
  }
  auto vbo1 = prepare(dvbo, num_points, point_size, offset);
  auto vbo2 = make_vbo();
  auto vao = make_vao();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo2);
  glBufferData(GL_ARRAY_BUFFER, static_cast<std::size_t>(num_points) * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
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
glm::vec2 minmax_x(gl_id vbo, uint32_t num_points, uint32_t point_size)
{
  return minmax(vbo, num_points, point_size, 0);
}

glm::vec2 minmax_y(gl_id vbo, uint32_t num_points, uint32_t point_size)
{
  return minmax(vbo, num_points, point_size, 1);
}

glm::vec2 minmax_z(gl_id vbo, uint32_t num_points, uint32_t point_size)
{
  return minmax(vbo, num_points, point_size, 2);
}

rect bounding_rect_2d(gl_id vbo, uint32_t num_points)
{
  auto bx = minmax_x(vbo, num_points, 2);
  if (bx.y - bx.x < 1e-8)
  {
    bx.x -= 1.0f;
    bx.y += 1.0f;
  }
  auto by = minmax_y(vbo, num_points, 2);
  if (by.y - by.x < 1e-8)
  {
    by.x -= 1.0f;
    by.y += 1.0f;
  }
  // auto bz = minmax_z(d);
  return rect{.lower_bounds = glm::vec3(bx.x, by.x, -1.0f),
              .upper_bounds = glm::vec3(bx.y, by.y, 1.0f)};
}

rect bounding_rect_3d(gl_id vbo, uint32_t num_points)
{
  auto bx = minmax_x(vbo, num_points, 3);
  if (bx.y - bx.x < 1e-8)
  {
    bx.x -= 1.0f;
    bx.y += 1.0f;
  }
  auto by = minmax_y(vbo, num_points, 3);
  if (by.y - by.x < 1e-8)
  {
    by.x -= 1.0f;
    by.y += 1.0f;
  }
  auto bz = minmax_z(vbo, num_points, 3);
  if (bz.y - bz.x < 1e-8)
  {
    bz.x -= 1.0f;
    bz.y += 1.0f;
  }
  return rect{.lower_bounds = glm::vec3(bx.x, by.x, bz.x),
              .upper_bounds = glm::vec3(bx.y, by.y, bz.y)};
}
} // namespace explot
