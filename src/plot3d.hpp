#pragma once

#include "gl-handle.hpp"
#include "graph3d.hpp"
#include <glm/vec3.hpp>
#include "rect.hpp"
#include "coordinate_system_3d.hpp"
#include "legend.hpp"
#include <vector>

namespace explot
{
struct plot3d final
{
  explicit plot3d(const plot_command_3d &cmd);
  std::vector<graph3d> graphs;
  tics_desc phase_space;
  coordinate_system_3d cs;
  legend legend;

private:
  plot3d(const plot_command_3d &cmd, std::span<const line_type> lts);
};

void update(const plot3d &plot, const glm::vec3 &view_origin, const glm::mat4 &view_rotation,
            const rect &screen);
void draw(const plot3d &plot);
} // namespace explot
