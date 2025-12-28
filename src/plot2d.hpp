#pragma once

#include <vector>
#include "rect.hpp"
#include "graph2d.hpp"
#include "commands.hpp"
#include "legend.hpp"
#include "csv.hpp"

namespace explot
{
struct plot2d
{
  plot2d(const plot_command_2d &cmd);
  rect phase_space;
  std::vector<graph2d> graphs;
  legend legend;
  time_point timebase;
};

void update(const plot2d &plot, const rect &screen, const rect &view);
void draw(const plot2d &plot);
} // namespace explot
