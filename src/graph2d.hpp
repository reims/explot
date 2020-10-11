#pragma once

#include <memory>
#include "line_drawing.hpp"
#include "data.hpp"
#include "minmax.hpp"
#include "point_drawing.hpp"
#include <variant>
#include "commands.hpp"

namespace explot
{
struct graph2d final
{
  using state = std::variant<points_2d_state, line_strip_state_2d>;
  state graph;
  data_desc data;

  graph2d() = default;
  graph2d(data_desc data, mark_type mark);
  graph2d(graph2d &&) = default;

  graph2d &operator=(graph2d &&) = default;
};

// graph2d make_graph2d(std::shared_ptr<data_desc> data, mark_type mark);
void draw(const graph2d &graph, float width, const glm::mat4 &view_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color);
rect bounding_rect(const graph2d &graph);
glm::vec2 minmax_x(const graph2d &graph);
glm::vec2 minmax_y(const graph2d &graph);
} // namespace explot
