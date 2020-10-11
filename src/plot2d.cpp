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
  auto bounding = std::optional<rect>();
  for (std::size_t i = 0; i < cmd.graphs.size(); ++i)
  {
    const auto &g = cmd.graphs[i];
    graphs[i] = graph2d(data_from_source(g.data), g.mark);
    auto br = bounding_rect(graphs[i]);
    if (bounding)
    {
      bounding = union_rect(bounding.value(), br);
    }
    else
    {
      bounding = br;
    }
  }

  return plot2d{bounding.value_or(clip_rect), std::move(graphs)};
}

void draw(const plot2d &plot, const glm::mat4 &view_to_screen, const glm::mat4 &screen_to_clip)
{
  assert(plot.graphs.size <= num_graph_colors);
  for (std::size_t i = 0; i < plot.graphs.size; ++i)
  {
    draw(plot.graphs[i], 2.0f, view_to_screen, screen_to_clip, graph_colors[i]);
  }
}
} // namespace explot
