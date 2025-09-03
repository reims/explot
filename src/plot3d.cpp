#include "plot3d.hpp"
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <numbers>
#include <fmt/format.h>
#include <cassert>
#include "data.hpp"
#include "gl-handle.hpp"
#include "minmax.hpp"

namespace
{
using namespace explot;
auto graphs_for_descs(const plot_command_3d &plot, std::span<const line_type> lts,
                      std::vector<std::tuple<vbo_handle, data_desc>> &&data)
{
  const auto &descs = plot.graphs;
  auto result = std::vector<graph3d>();
  result.reserve(descs.size());

  for (auto i = 0U; i < descs.size(); ++i)
  {
    result.emplace_back(std::move(std::get<0>(data[i])), std::get<1>(data[i]), descs[i].mark,
                        lts[i]);
  }

  return result;
}

auto bounding_rect(const graph3d &g) { return bounding_rect_3d(g.vbo, g.num_points); }

auto bounding_rect_for_graphs(std::span<const graph3d> graphs)
{
  if (graphs.empty())
  {
    return tics_desc{};
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
plot3d::plot3d(const plot_command_3d &cmd, std::span<const line_type> lts)
    : graphs(graphs_for_descs(cmd, lts, data_for_plot(cmd))),
      phase_space(bounding_rect_for_graphs(graphs)), cs(phase_space, 7), legend(cmd.graphs, lts),
      ubo(make_vbo())
{
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferRange(GL_UNIFORM_BUFFER, 0, ubo, 0, sizeof(glm::mat4));
  glBindBufferRange(GL_UNIFORM_BUFFER, 1, ubo, sizeof(glm::mat4), sizeof(glm::mat4));
  glBindBufferRange(GL_UNIFORM_BUFFER, 2, ubo, 2 * sizeof(glm::mat4), sizeof(glm::mat4));
}

plot3d::plot3d(const plot_command_3d &cmd) : plot3d(cmd, resolve_line_types(cmd.graphs)) {}

void update(const plot3d &plot, const glm::vec3 &view_origin, const glm::mat4 &view_rotation,
            const rect &screen)
{
  const auto width = static_cast<float>(screen.upper_bounds.x - screen.lower_bounds.x);
  const auto height = static_cast<float>(screen.upper_bounds.y - screen.lower_bounds.y);
  const auto phase_to_std_view = transform(plot.phase_space.bounding_rect, clip_rect);
  const auto view_to_clip =
      glm::perspectiveFov(std::numbers::pi_v<float> / 4.0f, width, height, 0.5f, 10.0f);
  const auto pre_translation = glm::translate(glm::identity<glm::mat4>(), view_origin);
  const auto phase_to_view = view_rotation * pre_translation * phase_to_std_view;

  glm::mat4 ufs[] = {view_to_clip * phase_to_view, transform(screen, clip_rect),
                     transform(clip_rect, screen)};

  glBindBuffer(GL_UNIFORM_BUFFER, plot.ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ufs), ufs);
  update(plot.legend, screen);
  update(plot.cs, ufs[0], ufs[2], ufs[1]);
}

void draw(const plot3d &plot)
{
  for (const auto &g : plot.graphs)
  {
    draw(g);
  }
  draw(plot.cs);
  draw(plot.legend);
}
} // namespace explot
