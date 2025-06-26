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
              return points_2d_state(std::move(data), lt.width, lt.color, 9);
            case mark_type_2d::lines:
              if (lt.dash_type)
              {
                return dashed_line_strip_state_2d(std::move(data), lt.dash_type->segments, lt.width,
                                                  lt.color);
              }
              else
              {
                return line_strip_state_2d(std::move(data), lt.width, lt.color);
              }
            case mark_type_2d::impulses:
              return impulses_state(std::move(data), lt.width, lt.color);
            }
            throw "bad";
          }()),
      lt(lt)
{
}

void update(const graph2d &graph)
{
  std::visit(overload([&](const points_2d_state &s) {}, [&](const line_strip_state_2d &s) {},
                      [&](const dashed_line_strip_state_2d &s) { update(s); },
                      [&](const impulses_state &s) {}),
             graph.graph);
}

void draw(const graph2d &graph)
{
  std::visit(overload([&](const points_2d_state &s) { draw(s); }, [&](const line_strip_state_2d &s)
                      { draw(s); }, [&](const dashed_line_strip_state_2d &s) { draw(s); },
                      [&](const impulses_state &s) { draw(s); }),
             graph.graph);
}

} // namespace explot
