#pragma once

#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "rect.hpp"
#include "font_atlas.hpp"
#include <vector>

namespace explot
{
struct coordinate_system_3d final
{
  glm::mat4 scale_to_phase;
  vbo_handle vbo;
  lines_state_3d lines;
  font_atlas font;
  std::vector<gl_string> xlabels;
  std::vector<gl_string> ylabels;
  std::vector<gl_string> zlabels;

  coordinate_system_3d(const tics_desc &phase_space, std::uint32_t num_ticks);
};

void update(coordinate_system_3d &cs, const transforms_3d &transforms);
void draw(const coordinate_system_3d &cs);
} // namespace explot
