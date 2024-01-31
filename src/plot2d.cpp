#include "plot2d.hpp"
#include "gl-handle.hpp"
#include <GL/glew.h>
#include <array>
#include <cmath>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <cassert>
#include "colors.hpp"

namespace
{
} // namespace

namespace explot
{
plot2d make_plot2d(const plot_command_2d &cmd)
{
  auto graphs = make_unique_span<graph2d>(cmd.graphs.size());
  auto lts = resolve_line_types(cmd.graphs);
  auto data = data_for_plot(cmd);
  auto bounding = std::optional<rect>();
  for (std::size_t i = 0; i < cmd.graphs.size(); ++i)
  {
    const auto &g = cmd.graphs[i];
    graphs[i] = graph2d(std::move(data[i]), g.mark, lts[i]);
    auto br = bounding_rect(graphs[i]);
    bounding = union_rect(bounding.value_or(br), br);
  }

  return plot2d{bounding.value_or(clip_rect), std::move(graphs), legend(cmd.graphs, lts)};
}

void draw(const plot2d &plot, const rect &screen, const rect &view)
{
  const auto view_to_screen = transform(view, screen);
  const auto screen_to_clip = transform(screen, clip_rect);
  assert(plot.graphs.size <= num_graph_colors);
  for (std::size_t i = 0; i < plot.graphs.size; ++i)
  {
    draw(plot.graphs[i], view_to_screen, screen_to_clip);
  }
  draw(plot.legend, screen, screen_to_clip);
}
} // namespace explot
