#include "graph3d.hpp"
#include "data.hpp"
#include "line_type.hpp"
#include "overload.hpp"
#include "surfaces.hpp"
#include <variant>

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
  case mark_type_3d::surface:
    assert(std::holds_alternative<grid_data_desc>(data));
    return surface_lines(vbo, std::get<grid_data_desc>(data), lt);
  }
  throw 0;
}

uint32_t get_num_points(const data_desc &d)
{
  return std::visit(overload([](const seq_data_desc &s) { return s.num_points; },
                             [](const grid_data_desc &g) { return g.num_columns * g.num_rows; }),
                    d);
}
} // namespace

namespace explot
{
graph3d::graph3d(vbo_handle v, const data_desc &data, mark_type_3d mark, line_type lt)
    : vbo(std::move(v)), num_points(get_num_points(data)), graph(make_state(vbo, data, mark, lt)),
      lt(lt)
{
}

void draw(const graph3d &graph)
{
  std::visit(overload([](const line_strip_state_3d &lines) { draw(lines); },
                      [](const points_3d_state &points) { draw(points); },
                      [](const surface_lines &s) { draw(s); }),
             graph.graph);
}

} // namespace explot
