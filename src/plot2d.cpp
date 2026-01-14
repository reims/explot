#include "plot2d.hpp"
#include <GL/glew.h>
#include <array>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <cassert>
#include "minmax.hpp"

namespace
{
using namespace explot;

constexpr auto lower_margin = glm::vec3(100.0f, 50.0f, 0.0f);
constexpr auto upper_margin = glm::vec3(50.0f, 20.0f, 0.0f);

void set_viewport(const rect &r)
{
  glViewport(static_cast<GLint>(r.lower_bounds.x), static_cast<GLint>(r.lower_bounds.y),
             static_cast<GLsizei>(r.upper_bounds.x - r.lower_bounds.x),
             static_cast<GLsizei>(r.upper_bounds.y - r.lower_bounds.y));
}

} // namespace

namespace explot
{

plot2d::plot2d(const plot_command_2d &cmd)
    : legend(cmd.graphs), cs(5, 9, 2, time_point(), cmd.xdata, cmd.timefmt)
{
  graphs.reserve(cmd.graphs.size());
  auto [data, tb] = data_for_plot(cmd);
  auto bounding = std::optional<rect>();
  for (std::size_t i = 0; i < cmd.graphs.size(); ++i)
  {
    const auto &g = cmd.graphs[i];
    auto &[vbo, desc] = data[i];
    auto br = bounding_rect_2d(vbo, desc.num_points);
    graphs.emplace_back(std::move(vbo), desc, g.mark, g.line_type);
    bounding = union_rect(bounding.value_or(br), br);
  }
  phase_space = scale2d(bounding.value_or(clip_rect), 1.1f);
  cs.timebase = tb;
}

void draw(const plot2d &plot)
{
  set_viewport(plot.plot_screen);
  for (const auto &g : plot.graphs)
  {
    draw(g);
  }
  draw(plot.legend);
  set_viewport(plot.screen);
  draw(plot.cs);
}

void update_screen(plot2d &plot, const rect &screen)
{
  plot.screen = screen;
  plot.plot_screen = remove_margin(screen, lower_margin, upper_margin);
  transforms_2d transforms = {.phase_to_screen = transform(plot.view, plot.screen),
                              .screen_to_clip = transform(plot.screen, clip_rect)};

  update(plot.legend, screen, transforms.screen_to_clip);
  update_screen(plot.cs, plot.plot_screen, transforms);
  for (const auto &g : plot.graphs)
  {
    update(g, transforms);
  }
}

void update_view(plot2d &plot, const rect &view)
{
  auto rounded_view = round_for_ticks_2d(view, 5, 2);
  plot.view = rounded_view.bounding_rect;
  transforms_2d transforms = {.phase_to_screen = transform(plot.view, plot.screen),
                              .screen_to_clip = transform(plot.screen, clip_rect)};

  update_view(plot.cs, rounded_view, transforms);
  for (const auto &g : plot.graphs)
  {
    update(g, transforms);
  }
}

} // namespace explot
