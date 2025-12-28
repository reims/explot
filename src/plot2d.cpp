#include "plot2d.hpp"
#include <GL/glew.h>
#include <array>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <cassert>
#include "minmax.hpp"

namespace
{
} // namespace

namespace explot
{

plot2d::plot2d(const plot_command_2d &cmd) : legend(cmd.graphs)
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
  phase_space = bounding.value_or(clip_rect);
  timebase = tb;
}

void draw(const plot2d &plot)
{
  for (const auto &g : plot.graphs)
  {
    draw(g);
  }
  draw(plot.legend);
}

void update(const plot2d &plot, const rect &screen, const rect &view)
{
  transforms_2d transforms = {.phase_to_screen = transform(view, screen),
                              .screen_to_clip = transform(screen, clip_rect)};

  update(plot.legend, screen, transforms.screen_to_clip);
  for (const auto &g : plot.graphs)
  {
    update(g, transforms);
  }
}
} // namespace explot
