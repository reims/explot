#pragma once

#include "data.hpp"
#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "rect.hpp"

namespace explot
{
struct drag_render_state final
{
  line_strip_state_2d lines;
  vbo_handle vbo;

  drag_render_state();

  drag_render_state(std::tuple<vbo_handle, seq_data_desc> &&t);
};

void draw(const drag_render_state &s, const rect &d, const rect &screen);
} // namespace explot
