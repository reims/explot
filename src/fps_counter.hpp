#pragma once

#include "gl-handle.hpp"
#include "rect.hpp"

namespace explot
{
struct fps_counter final
{
  texture_handle texture;
  vao_handle vao;
  program_handle program;
  vbo_handle vbo;
  // vbo_handle debug_vbo;
};

fps_counter make_fps_counter();
void draw(const fps_counter &counter, const rect &r, const glm::mat4 &screen_to_clip);
} // namespace explot
