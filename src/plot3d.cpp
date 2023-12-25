#include "plot3d.hpp"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <numbers>
#include <fmt/format.h>
#include "colors.hpp"
#include <cassert>
#include "data.hpp"

namespace
{
using namespace explot;
auto graphs_for_descs(std::span<const graph_desc_3d> descs, const plot_command_3d &plot)
{
  auto result = make_unique_span<graph3d>(descs.size());

  for (auto i = 0U; i < descs.size(); ++i)
  {
    auto data = data_for_chart_3d(descs[i].data, plot);
    result[i] = graph3d(std::move(data), descs[i].mark);
  }

  return result;
}

auto bounding_rect_for_graphs(std::span<const graph3d> graphs)
{
  if (graphs.empty())
  {
    return rect{};
  }
  else
  {
    auto result = bounding_rect(graphs[0]);
    for (auto i = 1U; i < graphs.size(); ++i)
    {
      result = union_rect(result, bounding_rect(graphs[i]));
    }
    return round_for_ticks_3d(result, 7, 2);
  }
}
} // namespace

namespace explot
{
plot3d::plot3d(const plot_command_3d &cmd)
    : graphs(graphs_for_descs(cmd.graphs, cmd)), phase_space(bounding_rect_for_graphs(graphs)),
      cs(phase_space, 7), legend(cmd.graphs)
{
}

void draw(const plot3d &plot, const glm::vec3 &view_origin, const glm::mat4 &view_rotation,
          const rect &screen)
{
  assert(plot.graphs.size <= num_graph_colors);
  const auto width = static_cast<float>(screen.upper_bounds.x - screen.lower_bounds.x);
  const auto height = static_cast<float>(screen.upper_bounds.y - screen.lower_bounds.y);
  const auto phase_to_std_view = transform(plot.phase_space, clip_rect);
  const auto view_to_clip =
      glm::perspectiveFov(std::numbers::pi_v<float> / 2.0f, width, height, 0.0001f, 10.0f);
  const auto pre_translation = glm::translate(glm::identity<glm::mat4>(), -view_origin);
  const auto phase_to_view = view_rotation * pre_translation * phase_to_std_view;

  const auto screen_to_clip = transform(screen, clip_rect);
  const auto clip_to_screen = transform(clip_rect, screen);
  for (auto i = 0U; i < plot.graphs.size; ++i)
  {
    draw(plot.graphs[i], phase_to_view, view_to_clip, clip_to_screen, screen_to_clip,
         graph_colors[i]);
  }
  draw(plot.cs, view_to_clip * phase_to_view, clip_to_screen, screen_to_clip);
  draw(plot.legend, screen, screen_to_clip);
}
} // namespace explot
