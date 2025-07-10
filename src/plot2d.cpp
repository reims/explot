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

plot2d::plot2d(const plot_command_2d &cmd) : plot2d(cmd, resolve_line_types(cmd.graphs)) {}
plot2d::plot2d(const plot_command_2d &cmd, const std::vector<line_type> &lts)
    : legend(cmd.graphs, lts), transforms(make_vbo())
{
  graphs.reserve(cmd.graphs.size());
  auto [data, tb] = data_for_plot(cmd);
  auto bounding = std::optional<rect>();
  for (std::size_t i = 0; i < cmd.graphs.size(); ++i)
  {
    const auto &g = cmd.graphs[i];
    auto &[vbo, desc] = data[i];
    auto br = bounding_rect_2d(vbo, desc.num_points);
    graphs.emplace_back(std::move(vbo), desc, g.mark, lts[i]);
    bounding = union_rect(bounding.value_or(br), br);
  }
  phase_space = bounding.value_or(clip_rect);
  timebase = tb;

  glBindBufferRange(GL_UNIFORM_BUFFER, 0, transforms, 0, sizeof(glm::mat4));
  glBindBufferRange(GL_UNIFORM_BUFFER, 1, transforms, sizeof(glm::mat4), sizeof(glm::mat4));
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
  const glm::mat4 matrices[] = {transform(view, screen), transform(screen, clip_rect)};
  glBindBuffer(GL_UNIFORM_BUFFER, plot.transforms);
  glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), matrices, GL_DYNAMIC_DRAW);
  update(plot.legend, screen);
  for (const auto &g : plot.graphs)
  {
    update(g);
  }
}
} // namespace explot
