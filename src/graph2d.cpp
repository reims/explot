#include "graph2d.hpp"
#include "overload.hpp"

namespace explot
{
graph2d::graph2d(vbo_handle vbo, const seq_data_desc &d, mark_type_2d mark, line_type lt)
    : vbo(std::move(vbo)), graph(
                               [&]() -> typename graph2d::state
                               {
                                 switch (mark)
                                 {
                                 case mark_type_2d::points:
                                   return points_2d_state(this->vbo, d, lt.width, lt.color, 9);
                                 case mark_type_2d::lines:
                                   if (lt.dash_type)
                                   {
                                     return dashed_line_strip_state_2d(
                                         this->vbo, d, lt.dash_type->segments, lt.width, lt.color);
                                   }
                                   else
                                   {
                                     return line_strip_state_2d(this->vbo, d, lt.width, lt.color);
                                   }
                                 case mark_type_2d::impulses:
                                   return impulses_state(this->vbo, d, lt.width, lt.color);
                                 }
                                 throw "bad";
                               }()),
      lt(lt)
{
}

void update(const graph2d &graph, const transforms_2d &transforms)
{
  std::visit(overload([&](const points_2d_state &s) { update(s, transforms); },
                      [&](const line_strip_state_2d &s) { update(s, transforms); },
                      [&](const dashed_line_strip_state_2d &s) { update(s, transforms); },
                      [&](const impulses_state &s) { update(s, transforms); }),
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
