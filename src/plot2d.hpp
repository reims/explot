#pragma once

#include <memory>
#include <vector>
#include "rect.hpp"
#include "coordinate_system_2d.hpp"
#include "graph2d.hpp"
#include "commands.hpp"
#include "unique_span.hpp"
#include "legend.hpp"
#include <chrono>
#include "csv.hpp"

namespace explot
{
struct plot2d final
{
  rect phase_space;
  unique_span<graph2d> graphs;
  legend legend;
  time_point timebase;
};

plot2d make_plot2d(const plot_command_2d &cmd);
void draw(const plot2d &plot, const rect &screen, const rect &view);
} // namespace explot
