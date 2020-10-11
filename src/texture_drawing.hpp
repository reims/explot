#pragma once

#include "gl-handle.hpp"

namespace cpp_vis
{
struct texture_state final
{
  vbo_handle vbo;
  vao_handle vao;
  program_handle program;
  unsigned int texture;

  explicit texture_state(unsigned int texture);
};

void draw(const texture_state &state);
} // namespace cpp_vis
