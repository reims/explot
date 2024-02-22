#pragma once

#include <memory>
#include "line_drawing.hpp"
#include "data.hpp"
#include "minmax.hpp"
#include "point_drawing.hpp"
#include <variant>
#include "commands.hpp"
#include "line_type.hpp"
#include "impulse.hpp"

namespace explot
{
struct graph2d final
{
  using state = std::variant<points_2d_state, line_strip_state_2d, impulses_state>;
  state graph;
  line_type lt;
  graph2d() = default;
  graph2d(data_desc data, mark_type_2d mark, line_type line_type);
  graph2d(graph2d &&) = default;

  graph2d &operator=(graph2d &&) = default;
};

// graph2d make_graph2d(std::shared_ptr<data_desc> data, mark_type mark);
void draw(const graph2d &graph, const glm::mat4 &view_to_screen, const glm::mat4 &screen_to_clip);
} // namespace explot
