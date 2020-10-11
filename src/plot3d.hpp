#pragma once

#include "graph3d.hpp"
#include <glm/vec3.hpp>
#include "unique_span.hpp"
#include "rect.hpp"
#include "coordinate_system_3d.hpp"

namespace explot
{
struct plot3d final
{
  explicit plot3d(graph3d);
  explicit plot3d(const plot_command_3d &cmd);
  unique_span<graph3d> graphs;
  rect phase_space;
  coordinate_system_3d cs;
};

void draw(const plot3d &plot, const glm::vec3 &view_origin, const glm::mat4 &view_rotation,
          const rect &screen);
} // namespace explot
