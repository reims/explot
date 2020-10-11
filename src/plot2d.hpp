#pragma once

#include <memory>
#include <vector>
#include "rect.hpp"
#include "coordinate_system_2d.hpp"
#include "graph2d.hpp"
#include "commands.hpp"
#include "unique_span.hpp"

namespace explot
{
struct plot2d final
{
  rect phase_space;
  unique_span<graph2d> graphs;
};

/*
constant: samples, number of ticks, data
variable: range, screen space
*/
plot2d make_plot2d(const plot_command_2d &cmd);
void draw(const plot2d &plot, const glm::mat4 &view_to_screen, const glm::mat4 &screen_to_clip);
} // namespace explot
