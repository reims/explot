#pragma once
#include "line_drawing.hpp"

namespace explot
{
struct impulses_state
{
  explicit impulses_state(data_desc d, float width, const glm::vec4 &color);
  lines_state_2d lines;
};
void draw(const impulses_state &s);
} // namespace explot
