#include <vector>
#include "commands.hpp"
#include "layout.hpp"
#include "rect.hpp"
#include "settings.hpp"
#include "rx-renderers.hpp"

namespace
{
using namespace explot;
} // namespace

namespace explot
{

layout::layout()
    : rects(1, rect{.lower_bounds = {0.0f, 0.0f, 0.0f}, .upper_bounds = {1.0f, 1.0f, 1.0f}}),
      num_plots(0)
{
}

layout::~layout() { sub.unsubscribe(); }

void add_plot(layout &l, const plot_command_2d &cmd, rx::observable<unit> frames,
              rx::observable<rect> screen, rx::observe_on_one_worker rl)
{
  if (l.num_plots >= l.rects.size())
  {
    l.num_plots = 0;
  }
  if (l.num_plots == 0)
  {
    reset(l, settings::multiplot().rows, settings::multiplot().cols);
  }
  auto &relative_rect = l.rects[l.num_plots];
  auto plot_frames = plot_renderer(rl, frames, screen, relative_rect, cmd);
  plot_frames.subscribe(l.sub, [](unit) {});
  ++l.num_plots;
}

void add_plot(layout &l, const plot_command_3d &cmd, rx::observable<unit> frames,
              rx::observable<rect> screen, rx::observe_on_one_worker rl)
{
  if (l.num_plots >= l.rects.size())
  {
    l.num_plots = 0;
  }
  if (l.num_plots == 0)
  {
    reset(l, settings::multiplot().rows, settings::multiplot().cols);
  }
  auto &relative_rect = l.rects[l.num_plots];
  auto plot_frames = splot_renderer(rl, frames, screen, relative_rect, cmd);
  plot_frames.subscribe(l.sub, [](unit) {});
  ++l.num_plots;
}

void reset(layout &l, uint32_t rows, uint32_t columns)
{
  l.sub.unsubscribe();
  l.sub = rx::composite_subscription();
  l.rects.clear();
  l.rects.reserve(rows * columns);
  auto row_height = 1.0f / static_cast<float>(rows);
  auto column_width = 1.0f / static_cast<float>(columns);
  for (auto r = 0u; r < rows; ++r)
  {
    for (auto c = 0u; c < columns; ++c)
    {
      l.rects.emplace_back(glm::vec3(static_cast<float>(c) * column_width,
                                     static_cast<float>(rows - r - 1) * row_height, 0.0f),
                           glm::vec3(static_cast<float>(c + 1) * column_width,
                                     static_cast<float>(rows - r) * row_height, 1.0f));
    }
  }
}
} // namespace explot
