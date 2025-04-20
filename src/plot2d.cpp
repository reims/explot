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
plot2d make_plot2d(const plot_command_2d &cmd)
{
  auto graphs = std::vector<graph2d>();
  graphs.reserve(cmd.graphs.size());
  auto lts = resolve_line_types(cmd.graphs);
  auto [data, timebase] = data_for_plot(cmd);
  auto bounding = std::optional<rect>();
  for (std::size_t i = 0; i < cmd.graphs.size(); ++i)
  {
    const auto &g = cmd.graphs[i];
    auto br = bounding_rect_2d(data[i], 2);
    graphs.emplace_back(std::move(data[i]), g.mark, lts[i]);
    bounding = union_rect(bounding.value_or(br), br);
  }

  return plot2d{bounding.value_or(clip_rect), std::move(graphs), legend(cmd.graphs, lts), timebase};
}

void draw(const plot2d &plot, const rect &screen, const rect &view)
{
  const auto view_to_screen = transform(view, screen);
  const auto screen_to_clip = transform(screen, clip_rect);
  for (const auto &g : plot.graphs)
  {
    draw(g, view_to_screen, screen_to_clip);
  }
  draw(plot.legend, screen, screen_to_clip);
}
} // namespace explot
