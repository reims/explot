#pragma once
#include "gl-handle.hpp"
#include "data.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace explot
{
struct points_2d_state final
{
  vao_handle vao;
  program_handle program;
  std::uint32_t num_points;
  points_2d_state() = default;
  explicit points_2d_state(const data_desc &data);
};

void draw(const points_2d_state &state, float line_width, float point_width, const glm::vec4 &color,
          const glm::mat4 &view_to_screen, const glm::mat4 &screen_to_clip);

struct points_3d_state final
{
  vao_handle vao;
  program_handle program;
  std::uint32_t num_points;
  points_3d_state() = default;
  explicit points_3d_state(const data_desc &data);
};

void draw(const points_3d_state &state, float line_width, float point_width, const glm::vec4 &color,
          const glm::mat4 &phase_to_clip, const glm::mat4 &clip_to_screen,
          const glm::mat4 &screen_to_clip);

} // namespace explot
