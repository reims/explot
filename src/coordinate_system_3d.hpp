#pragma once

#include "line_drawing.hpp"
#include "data.hpp"
#include "rect.hpp"
#include "font_atlas.hpp"

namespace explot
{
struct coordinate_system_3d final
{
  glm::mat4 scale_to_phase;
  data_desc data;
  lines_state_3d lines;
  font_atlas font;
  unique_span<gl_string> xlabels;
  unique_span<gl_string> ylabels;
  unique_span<gl_string> zlabels;

  coordinate_system_3d(const rect &phase_space, std::uint32_t num_ticks);
};

void draw(const coordinate_system_3d &cs, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip);
} // namespace explot
