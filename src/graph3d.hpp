#pragma once
#include "data.hpp"
#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "point_drawing.hpp"
#include "commands.hpp"
#include <variant>
#include "line_type.hpp"

namespace explot
{
struct graph3d final
{
  using state = std::variant<line_strip_state_3d, points_3d_state>;
  explicit graph3d(vbo_handle vbo, const data_desc &data, mark_type_3d mark, line_type lt);
  vbo_handle vbo;
  state graph;
  line_type lt;
};

void draw(const graph3d &graph);
} // namespace explot
