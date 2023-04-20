#include "graph2d.hpp"
#include "minmax.hpp"
#include "overload.hpp"

namespace explot
{
graph2d::graph2d(data_desc data, mark_type mark)
    : graph(
        [&]() -> typename graph2d::state
        {
          switch (mark)
          {
          case mark_type::points:
            return points_2d_state(data);
          case mark_type::lines:
            return make_line_strip_state_2d(data);
          }
          throw "bad";
        }()),
      data(std::move(data))
{
}

void draw(const graph2d &graph, float width, const glm::mat4 &view_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  std::visit(overload([&](const points_2d_state &s)
                      { draw(s, width, 9.0f, color, view_to_screen, screen_to_clip); },
                      [&](const line_strip_state_2d &s)
                      { draw(s, width, view_to_screen, screen_to_clip, color); }),
             graph.graph);
}

rect bounding_rect(const graph2d &graph) { return bounding_rect_2d(graph.data); }
glm::vec2 minmax_x(const graph2d &graph) { return explot::minmax_x(graph.data); }
glm::vec2 minmax_y(const graph2d &graph) { return explot::minmax_y(graph.data); }
} // namespace explot
