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
  coordinate_system_2d(uint32_t num_tics, float tick_size, float width, time_point timebase,
                       data_type xdata, std::string timefmt);
  uint32_t num_tics;
  float tic_size;
  program_handle program_for_x_tics;
  program_handle program_for_y_tics;
  vao_handle vao_for_tics;
  vbo_handle vbo_for_axis;
  lines_state_2d axis;
  std::vector<gl_string> x_labels;
  std::vector<gl_string> y_labels;
  font_atlas atlas;
  time_point timebase;
  data_type xdata;
  std::string timefmt;
};

void update_view(coordinate_system_2d &cs, const tics_desc &view, const transforms_2d &t);
void update_screen(coordinate_system_2d &cs, const rect &plot_screen, const transforms_2d &t);

void draw(const coordinate_system_2d &coordinate_system);
} // namespace explot
