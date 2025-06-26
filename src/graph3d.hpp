#pragma once
#include "data.hpp"
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
  explicit graph3d(data_desc data, mark_type_3d mark, line_type lt);
  state graph;
  line_type lt;
};

void draw(const graph3d &graph);
} // namespace explot
