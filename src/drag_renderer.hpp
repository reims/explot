#pragma once

#include "line_drawing.hpp"
#include "rect.hpp"

namespace explot
{
struct drag_render_state final
{
  line_strip_state_2d lines;
};

drag_render_state make_drag_render_state();
void draw(const drag_render_state &s, const rect &d, const glm::mat4 &screen_to_clip, float width);
} // namespace explot
