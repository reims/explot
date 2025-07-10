#pragma once

#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "data.hpp"
#include "point_drawing.hpp"
#include <variant>
#include "commands.hpp"
#include "line_type.hpp"
#include "impulse.hpp"

namespace explot
{
struct graph2d final
{
  using state = std::variant<points_2d_state, line_strip_state_2d, dashed_line_strip_state_2d,
                             impulses_state>;
  vbo_handle vbo;
  state graph;
  line_type lt;
  graph2d() = default;
  graph2d(vbo_handle vbo, const seq_data_desc &data, mark_type_2d mark, line_type line_type);
};

void update(const graph2d &graph);
void draw(const graph2d &graph);
} // namespace explot
