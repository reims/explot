#include "legend.hpp"
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <unordered_set>
#include "commands.hpp"
#include "gl-handle.hpp"
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
  auto glyphs = std::string();
  for (const auto &g : graphs)
  {
    glyphs.append(g.title.begin(), g.title.end());
  }
  std::ranges::sort(glyphs);
  glyphs.erase(std::unique(glyphs.begin(), glyphs.end()), glyphs.end());
  // str.erase(std::remove(str.begin(), str.end(), ' ')

  return glyphs;
}

std::string glyphs_for_graphs(std::span<const graph_desc_3d> graphs)
{
  auto glyphs = std::unordered_set<char>();
  for (const auto &g : graphs)
  {
    glyphs.insert(g.title.begin(), g.title.end());
  }
  auto str = std::string(glyphs.begin(), glyphs.end());
  // str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
  return str;
}

seq_data_desc make_point_data(gl_id vbo)
{
  static constexpr float coord[] = {0.0f, 0.0f};
  return data_for_span(vbo, coord, 2);
}

seq_data_desc make_line_data(gl_id vbo)
{
  static constexpr float coords[] = {-0.8f, 0.0f, 0.8f, 0.0f};
  return data_for_span(vbo, coords, 2);
}

} // namespace

namespace explot
{

legend::legend(std::span<const graph_desc_2d> graphs, std::span<const line_type> lts)
    : font(make_font_atlas(glyphs_for_graphs(graphs), 10).value())
{
  titles.reserve(graphs.size());
  colors.reserve(graphs.size());
  marks.reserve(graphs.size());
  vbos.reserve(graphs.size());
  auto i = 0u;
  for (const auto &g : graphs)
  {
    auto &vbo = vbos.emplace_back(make_vbo());
    switch (g.mark)
    {
    case mark_type_2d::points:
      marks.emplace_back(
          points_2d_state(vbo, make_point_data(vbo), lts[i].width, lts[i].color, 9.0f));
      break;
    case mark_type_2d::impulses:
    case mark_type_2d::lines:
      marks.emplace_back(lines_state_2d(vbo, make_line_data(vbo), lts[i].width, lts[i].color));
      break;
    }
    titles.emplace_back(font, g.title, text_color);
    colors.push_back(graph_colors[(i++) % num_graph_colors]);
  }
}

legend::legend(std::span<const graph_desc_3d> graphs, std::span<const line_type> lts)
    : font(make_font_atlas(glyphs_for_graphs(graphs), 10).value())
{
  auto i = 0u;
  titles.reserve(graphs.size());
  colors.reserve(graphs.size());
  marks.reserve(graphs.size());
  vbos.reserve(graphs.size());
  for (const auto &g : graphs)
  {
    auto &vbo = vbos.emplace_back(make_vbo());
    switch (g.mark)
    {
    case mark_type_3d::points:
      marks.emplace_back(
          points_2d_state(vbo, make_point_data(vbo), lts[i].width, lts[i].color, 9.0f));
      break;
    case mark_type_3d::lines:
      marks.emplace_back(lines_state_2d(vbo, make_line_data(vbo), lts[i].width, lts[i].color));
      break;
    case mark_type_3d::surface:
    case explot::mark_type_3d::pm3d:
      marks.emplace_back(lines_state_2d(vbo, make_line_data(vbo), lts[i].width, lts[i].color));
      break;
    }
    titles.emplace_back(font, g.title, text_color);
    colors.push_back(lts[i++].color);
  }
}

void update(const legend &l, const rect &screen, const glm::mat4 &screen_to_clip)
{
  const auto text_width = std::ranges::max(std::views::transform(
      l.titles, [](const gl_string &s) { return s.upper_bounds.x - s.lower_bounds.x; }));
  const auto text_height = std::ranges::max(std::views::transform(
      l.titles, [](const gl_string &s) { return s.upper_bounds.y - s.lower_bounds.y; }));
  const auto start_of_mark = screen.upper_bounds.x - text_width - 30.f;
  assert(l.titles.size() == l.marks.size());
  for (auto i = 0u; i < l.titles.size(); ++i)
  {
    const auto &s = l.titles[i];
    const auto screen_rect = rect{
        .lower_bounds = {start_of_mark, screen.upper_bounds.y - (i + 1) * text_height - 10, -1.0f},
        .upper_bounds = {start_of_mark + 20.0f, screen.upper_bounds.y - i * text_height - 10,
                         1.0f}};
    auto mark_transforms = transforms_2d{.phase_to_screen = transform(clip_rect, screen_rect),
                                         .screen_to_clip = screen_to_clip};
    std::visit([&](const auto &m) { update(m, mark_transforms); }, l.marks[i]);
    update(s, {start_of_mark + 20, screen_rect.upper_bounds.y, 0}, {0.0f, 1.0f}, screen_to_clip);
  }
}

void draw(const legend &l)
{
  assert(l.titles.size() == l.marks.size());
  for (auto i = 0u; i < l.titles.size(); ++i)
  {
    const auto &m = l.marks[i];
    const auto &s = l.titles[i];
    std::visit(overload([&](const points_2d_state &p) { draw(p); },
                        [&](const lines_state_2d &ls) { draw(ls); }),
               m);
    draw(s);
  }
}
} // namespace explot
