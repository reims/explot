#pragma once

#include "line_drawing.hpp"
#include "rect.hpp"
#include "font_atlas.hpp"
#include <vector>

namespace explot
{
struct coordinate_system_3d final
{
  glm::mat4 scale_to_phase;
  lines_state_3d lines;
  font_atlas font;
  std::vector<gl_string> xlabels;
  std::vector<gl_string> ylabels;
  std::vector<gl_string> zlabels;

  coordinate_system_3d(const tics_desc &phase_space, std::uint32_t num_ticks);
};

void draw(const coordinate_system_3d &cs, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip);
} // namespace explot
