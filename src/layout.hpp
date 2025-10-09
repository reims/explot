#pragma once

#include <vector>
#include "rect.hpp"
#include "rx.hpp"
#include "commands.hpp"

namespace explot
{
struct layout
{
  std::vector<rect> rects;
  uint32_t num_plots;
  rx::composite_subscription sub;

  layout();
  ~layout();
};

void add_plot(layout &l, const plot_command_2d &cmd, rx::observable<unit> frames,
              rx::observable<rect> screen, rx::observe_on_one_worker rl);

void add_plot(layout &l, const plot_command_3d &cmd, rx::observable<unit> frames,
              rx::observable<rect> screen, rx::observe_on_one_worker rl);

void reset(layout &l, uint32_t rows, uint32_t columns);

} // namespace explot
