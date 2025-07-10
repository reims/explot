#include "graph3d.hpp"
#include "line_type.hpp"
#include "overload.hpp"

namespace
{
using namespace explot;
graph3d::state make_state(gl_id vbo, const data_desc &data, mark_type_3d mark, const line_type &lt)
{
  switch (mark)
  {
  case mark_type_3d::lines:
    return std::visit([&](const auto &d)
                      { return line_strip_state_3d(vbo, d, lt.width, lt.color); }, data);
  case mark_type_3d::points:
    return std::visit([&](const auto &d)
                      { return points_3d_state(vbo, d, lt.width, lt.color, 9.0f); }, data);
  }
  throw 0;
}
} // namespace

namespace explot
{
graph3d::graph3d(vbo_handle v, const data_desc &data, mark_type_3d mark, line_type lt)
    : vbo(std::move(v)), graph(make_state(vbo, data, mark, lt)), lt(lt)
{
}

void draw(const graph3d &graph)
{
  std::visit(overload([&](const line_strip_state_3d &lines) { draw(lines); },
                      [&](const points_3d_state &points) { draw(points); }),
             graph.graph);
}

} // namespace explot
