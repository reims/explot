#pragma once
#include "gl-handle.hpp"
#include "data.hpp"
#include "program.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace explot
{
struct points_2d_state
{
  vao_handle vao;
  program_handle program;
  draw_info data;
  points_2d_state() = default;
  explicit points_2d_state(gl_id vbo, const seq_data_desc &d, float width, const glm::vec4 &color,
                           float point_width, const uniform_bindings_2d &bds = {});
};

void draw(const points_2d_state &state);

struct points_3d_state final
{
  vao_handle vao;
  program_handle program;
  draw_info data;
  points_3d_state() = default;
  explicit points_3d_state(gl_id vbo, const seq_data_desc &d, float width, const glm::vec4 &color,
                           float point_width, const uniform_bindings_3d &bds = {});
  explicit points_3d_state(gl_id vbo, const grid_data_desc &d, float width, const glm::vec4 &color,
                           float point_width, const uniform_bindings_3d &bds = {});
};

void draw(const points_3d_state &state);

} // namespace explot
