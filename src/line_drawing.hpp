#pragma once

#include "gl-handle.hpp"
#include "data.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace explot
{
struct line_strip_state_2d final
{
  vao_handle vao = make_vao();
  program_handle program;
  std::uint32_t num_points_per_segment;
  std::uint32_t num_segments;
};

line_strip_state_2d make_line_strip_state_2d(const data_desc &d);
void draw(const line_strip_state_2d &state, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color);

struct line_strip_state_3d final
{
  vao_handle vao = make_vao();
  program_handle program;
  std::uint32_t num_points_per_segment;
  std::uint32_t num_segments;
};

line_strip_state_3d make_line_strip_state_3d(const data_desc &d);
void draw(const line_strip_state_3d &state, float width, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip, const glm::vec4 &color);

struct lines_state_2d final
{
  vao_handle vao = make_vao();
  program_handle program;
  std::uint32_t num_points;
};

lines_state_2d make_lines_state_2d(const data_desc &d);
void draw(const lines_state_2d &state, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color);

struct lines_state_3d final
{
  vao_handle vao = make_vao();
  program_handle program;
  std::uint32_t num_points;
};

lines_state_3d make_lines_state_3d(const data_desc &d);
void draw(const lines_state_3d &state, float width, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip, const glm::vec4 &color);
} // namespace explot
