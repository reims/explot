#include "graph3d.hpp"
#include "minmax.hpp"
#include "overload.hpp"
#include <fmt/format.h>

namespace
{
using namespace explot;
graph3d::state make_state(const data_desc &data, mark_type mark)
{
  switch (mark)
  {
  case mark_type::lines:
    return make_line_strip_state_3d(data);
  case mark_type::points:
    return points_3d_state(data);
  }
  throw 0;
}
} // namespace

namespace explot
{
graph3d::graph3d(data_desc data, mark_type mark, line_type lt)
    : graph(make_state(data, mark)), data(std::move(data)), lt(lt)
{
}

rect bounding_rect(const graph3d &graph) { return bounding_rect_3d(graph.data); }

void draw(const graph3d &graph, const glm::mat4 &phase_to_view, const glm::mat4 &view_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip)
{
  std::visit(overload(
                 [&](const line_strip_state_3d &lines)
                 {
                   draw(lines, graph.lt.width, view_to_clip * phase_to_view, clip_to_screen,
                        screen_to_clip, graph.lt.color);
                 },
                 [&](const points_3d_state &points)
                 {
                   draw(points, graph.lt.width, 9.0f, graph.lt.color, view_to_clip * phase_to_view,
                        clip_to_screen, screen_to_clip);
                 }),
             graph.graph);
}
} // namespace explot
