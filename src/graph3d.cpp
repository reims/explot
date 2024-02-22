#include "graph3d.hpp"
#include "minmax.hpp"
#include "overload.hpp"
#include <fmt/format.h>

namespace
{
using namespace explot;
graph3d::state make_state(data_desc data, mark_type_3d mark)
{
  switch (mark)
  {
  case mark_type_3d::lines:
    return make_line_strip_state_3d(std::move(data));
  case mark_type_3d::points:
    return points_3d_state(std::move(data));
  }
  throw 0;
}
} // namespace

namespace explot
{
graph3d::graph3d(data_desc data, mark_type_3d mark, line_type lt)
    : graph(make_state(std::move(data), mark)), lt(lt)
{
}

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
