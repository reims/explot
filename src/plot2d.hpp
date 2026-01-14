#pragma once

#include <vector>
#include "rect.hpp"
#include "graph2d.hpp"
#include "commands.hpp"
#include "legend.hpp"
#include "csv.hpp"
#include "coordinate_system_2d.hpp"

namespace explot
{
struct plot2d
{
  plot2d(const plot_command_2d &cmd);
  rect phase_space;
  std::vector<graph2d> graphs;
  legend legend;
  coordinate_system_2d cs;
  rect screen;
  rect plot_screen;
  rect view;
};

void update_screen(plot2d &plot, const rect &screen);
void update_view(plot2d &plot, const rect &view);

void draw(const plot2d &plot);
} // namespace explot
