#pragma once
#include "line_drawing.hpp"

namespace explot
{
struct impulses_state
{
  explicit impulses_state(data_desc d) : lines(make_lines_state_2d(reshape(std::move(d), 2))) {}
  lines_state_2d lines;
};
void draw(const impulses_state &s, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color);
} // namespace explot
