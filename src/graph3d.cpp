#include "graph3d.hpp"
#include "line_type.hpp"
#include "overload.hpp"

namespace
{
using namespace explot;
graph3d::state make_state(data_desc data, mark_type_3d mark, const line_type &lt)
{
  switch (mark)
  {
  case mark_type_3d::lines:
    return line_strip_state_3d(std::move(data), lt.width, lt.color);
  case mark_type_3d::points:
    return points_3d_state(std::move(data), lt.width, lt.color, 9.0f);
  }
  throw 0;
}
} // namespace

namespace explot
{
graph3d::graph3d(data_desc data, mark_type_3d mark, line_type lt)
    : graph(make_state(std::move(data), mark, lt)), lt(lt)
{
}

void draw(const graph3d &graph)
{
  std::visit(overload([&](const line_strip_state_3d &lines) { draw(lines); },
                      [&](const points_3d_state &points) { draw(points); }),
             graph.graph);
}

} // namespace explot
