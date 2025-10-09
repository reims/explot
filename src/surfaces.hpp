#pragma once

#include "data.hpp"
#include "gl-handle.hpp"
#include "line_type.hpp"
#include "program.hpp"
#include "line_drawing.hpp"
#include "line_type.hpp"

namespace explot
{
struct surface
{
  vao_handle vao;
  program_handle program;
  draw_info data;

  surface(gl_id vbo, const grid_data_desc &d, const glm::vec4 &color);
};

void draw(const surface &s);

struct surface_lines
{
  vbo_handle offsets;
  surface surface;
  line_strip_state_3d upper_lines;
  line_strip_state_3d lower_lines;

  surface_lines(gl_id vbo, const grid_data_desc &d, const line_type &lt);
};

struct pm3d_surface
{
  vao_handle vao;
  program_handle program;
  draw_info data;

  pm3d_surface(gl_id vbo, const grid_data_desc &d);
};

void update(const surface_lines &s, const transforms_3d &transforms);
void draw(const surface_lines &s);
void update(const pm3d_surface &s, const transforms_3d &transforms);
void draw(const pm3d_surface &s);
} // namespace explot
