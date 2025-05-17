#pragma once

#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "rect.hpp"
#include "font_atlas.hpp"
#include "csv.hpp"
#include <vector>

namespace explot
{
struct coordinate_system_2d final
{
  uint32_t num_ticks;
  rect bounding_rect;
  program_handle program_for_ticks;
  vao_handle vao_for_ticks;
  lines_state_2d axis;
  std::vector<gl_string> x_labels;
  std::vector<gl_string> y_labels;
  font_atlas atlas;
};

coordinate_system_2d make_coordinate_system_2d(const tics_desc &tics, uint32_t num_ticks,
                                               time_point timebase);
void draw(const coordinate_system_2d &coordinate_system, const glm::mat4 &view_to_screen,
          const glm::mat4 &screen_to_clip, float width, float tick_size);
} // namespace explot
