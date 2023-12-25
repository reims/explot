#include "legend.hpp"
#include <string>
#include <glm/glm.hpp>
#include <unordered_set>
#include "rect.hpp"
#include "overload.hpp"
#include <algorithm>
#include <ranges>
#include <fmt/format.h>
#include "colors.hpp"

namespace
{
using namespace explot;

std::string glyphs_for_graphs(std::span<const graph_desc_2d> graphs)
{
  auto glyphs = std::unordered_set<char>();
  for (const auto &g : graphs)
  {
    glyphs.insert(g.title.begin(), g.title.end());
  }
  auto str = std::string(glyphs.begin(), glyphs.end());
  str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
  return str;
}

std::string glyphs_for_graphs(std::span<const graph_desc_3d> graphs)
{
  auto glyphs = std::unordered_set<char>();
  for (const auto &g : graphs)
  {
    glyphs.insert(g.title.begin(), g.title.end());
  }
  auto str = std::string(glyphs.begin(), glyphs.end());
  str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
  return str;
}

data_desc make_point_data()
{
  static constexpr float coord[] = {0.0f, 0.0f, 0.0f};
  return data_for_span(coord);
}

data_desc make_line_data()
{
  static constexpr float coords[] = {-0.8f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f};
  return data_for_span(coords);
}

} // namespace

namespace explot
{

legend::legend(std::span<const graph_desc_2d> graphs)
    : font(make_font_atlas(glyphs_for_graphs(graphs)).value()), point_data(make_point_data()),
      line_data(make_line_data())
{
  int i = 0;
  for (const auto &g : graphs)
  {
    switch (g.mark)
    {
    case mark_type::points:
      marks.push_back(points_2d_state(point_data));
      break;
    case mark_type::lines:
      marks.push_back(make_lines_state_2d(line_data));
      break;
    }
    titles.push_back(make_gl_string(font, g.title));
    colors.push_back(graph_colors[(i++) % num_graph_colors]);
  }
}

legend::legend(std::span<const graph_desc_3d> graphs)
    : font(make_font_atlas(glyphs_for_graphs(graphs)).value()), point_data(make_point_data()),
      line_data(make_line_data())
{
  int i = 0;
  for (const auto &g : graphs)
  {
    switch (g.mark)
    {
    case mark_type::points:
      marks.push_back(points_2d_state(point_data));
      break;
    case mark_type::lines:
      marks.push_back(make_lines_state_2d(line_data));
      break;
    }
    titles.push_back(make_gl_string(font, g.title));
    colors.push_back(graph_colors[(i++) % num_graph_colors]);
  }
}

void draw(const legend &l, const rect &screen, const glm::mat4 &screen_to_clip)
{
  const auto scale = 0.5f;
  const auto text_width =
      std::ranges::max(std::views::transform(l.titles, [](const gl_string &s)
                                             { return s.upper_bounds.x - s.lower_bounds.x; }))
      * scale;
  const auto text_height =
      std::ranges::max(std::views::transform(l.titles, [](const gl_string &s)
                                             { return s.upper_bounds.y - s.lower_bounds.y; }))
      * scale;
  const auto start_of_mark = screen.upper_bounds.x - text_width - 20.f;
  assert(l.titles.size() == l.marks.size());
  for (auto i = 0u; i < l.titles.size(); ++i)
  {
    const auto &m = l.marks[i];
    const auto &s = l.titles[i];
    const auto screen_rect = rect{
        .lower_bounds = {start_of_mark, screen.upper_bounds.y - (i + 1) * text_height, -1.0f},
        .upper_bounds = {start_of_mark + 20.0f, screen.upper_bounds.y - i * text_height, 1.0f}};
    const auto view_to_screen = transform(clip_rect, screen_rect);
    std::visit(overload([&](const points_2d_state &p)
                        { draw(p, 1.00f, 5.0f, l.colors[i], view_to_screen, screen_to_clip); },
                        [&](const lines_state_2d &ls)
                        { draw(ls, 1.0f, view_to_screen, screen_to_clip, l.colors[i]); }),
               m);
    draw(s, screen_to_clip, {start_of_mark + 20.0f, screen_rect.lower_bounds.y}, text_color, scale);
  }
}
} // namespace explot
