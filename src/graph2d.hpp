#pragma once

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
  state graph;
  line_type lt;
  graph2d() = default;
  graph2d(data_desc data, mark_type_2d mark, line_type line_type);
  graph2d(graph2d &&) = default;

  graph2d &operator=(graph2d &&) = default;
};

void update(const graph2d &graph);
void draw(const graph2d &graph);
} // namespace explot
