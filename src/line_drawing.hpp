#pragma once

#include "gl-handle.hpp"
#include "data.hpp"
#include "program.hpp"
#include <glm/ext/vector_float4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace explot
{
struct line_strip_state_2d final
{
  line_strip_state_2d(gl_id vbo, const seq_data_desc &d, float width, const glm::vec4 &color);
  vao_handle vao;
  program_handle program;
  draw_info data;
};

void update(const line_strip_state_2d &state, const transforms_2d &transforms);
void draw(const line_strip_state_2d &state);

struct line_strip_state_3d final
{
  line_strip_state_3d(gl_id vbo, const seq_data_desc &d, float width, const glm::vec4 &color);
  line_strip_state_3d(gl_id vbo, const grid_data_desc &d, float width, const glm::vec4 &color);
  line_strip_state_3d(gl_id vbo, const grid_data_desc &d, gl_id offsets, float factor, float width,
                      const glm::vec4 &color);

  vao_handle vao;
  program_handle program;
  draw_info data;
};
void update(const line_strip_state_3d &state, const transforms_3d &transforms);
void draw(const line_strip_state_3d &state);

struct lines_state_2d final
{
  lines_state_2d(gl_id vbo, const seq_data_desc &d, float width, const glm::vec4 &color);
  vao_handle vao = make_vao();
  program_handle program;
  draw_info data;
};

void update(const lines_state_2d &state, const transforms_2d &transforms);
void draw(const lines_state_2d &state);

struct dashed_line_strip_state_2d
{
  explicit dashed_line_strip_state_2d(gl_id vbo, const seq_data_desc &data,
                                      const std::vector<std::pair<uint32_t, uint32_t>> &dash_type,
                                      float width, const glm::vec4 &color);

  gl_id vbo;
  vao_handle vao;
  program_handle program;
  draw_info data;
  vbo_handle curve_length;
};
void update(const dashed_line_strip_state_2d &state, const transforms_2d &transforms);
void draw(const dashed_line_strip_state_2d &state);

struct lines_state_3d final
{
  lines_state_3d(gl_id vbo, const seq_data_desc &d, float width, const glm::vec4 &color);
  vao_handle vao;
  program_handle program;
  draw_info data;
};

void update(const lines_state_3d &state, const transforms_3d &transforms);
void draw(const lines_state_3d &state);
} // namespace explot
