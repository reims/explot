#pragma once

#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "rect.hpp"

namespace explot
{
struct drag_render_state final
{
  line_strip_state_2d lines;
  vbo_handle ubo;

  drag_render_state();
};

void draw(const drag_render_state &s, const rect &d);
} // namespace explot
