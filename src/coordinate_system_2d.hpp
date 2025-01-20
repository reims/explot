#pragma once

#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "rect.hpp"
#include "font_atlas.hpp"
#include "csv.hpp"
#include "unique_span.hpp"

namespace explot
{
struct coordinate_system_2d final
{
  int num_ticks;
  rect bounding_rect;
  program_handle program_for_ticks;
  vao_handle vao_for_ticks;
  lines_state_2d axis;
  unique_span<gl_string> x_labels;
  unique_span<gl_string> y_labels;
  font_atlas atlas;
};

coordinate_system_2d make_coordinate_system_2d(const rect &bounding_rect, int num_ticks,
                                               time_point timebase);
void draw(const coordinate_system_2d &coordinate_system, const glm::mat4 &view_to_screen,
          const glm::mat4 &screen_to_clip, float width, float tick_size);
} // namespace explot
