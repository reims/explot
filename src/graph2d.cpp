#include "graph2d.hpp"
#include "overload.hpp"

namespace explot
{
graph2d::graph2d(data_desc data, mark_type_2d mark, line_type lt)
    : graph(
          [&]() -> typename graph2d::state
          {
            switch (mark)
            {
            case mark_type_2d::points:
              return points_2d_state(std::move(data));
            case mark_type_2d::lines:
              if (lt.dash_type)
              {
                return dashed_line_strip_state_2d(std::move(data), lt.dash_type->segments);
              }
              else
              {
                return make_line_strip_state_2d(std::move(data));
              }
            case mark_type_2d::impulses:
              return impulses_state(std::move(data));
            }
            throw "bad";
          }()),
      lt(lt)
{
}

void draw(const graph2d &graph, const glm::mat4 &view_to_screen, const glm::mat4 &screen_to_clip)
{
  std::visit(
      overload([&](const points_2d_state &s)
               { draw(s, graph.lt.width, 9.0f, graph.lt.color, view_to_screen, screen_to_clip); },
               [&](const line_strip_state_2d &s)
               { draw(s, graph.lt.width, view_to_screen, screen_to_clip, graph.lt.color); },
               [&](const dashed_line_strip_state_2d &s)
               { draw(s, graph.lt.width, view_to_screen, screen_to_clip, graph.lt.color); },
               [&](const impulses_state &s)
               { draw(s, graph.lt.width, view_to_screen, screen_to_clip, graph.lt.color); }),
      graph.graph);
}

} // namespace explot
