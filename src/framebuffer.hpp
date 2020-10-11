#pragma once

#include "gl-handle.hpp"
#include "rect.hpp"

namespace cpp_vis
{
struct screen_buffer final
{
  texture_handle color_attachment;
  fbo_handle fbo;

  explicit screen_buffer(const rect &screen);
};
} // namespace cpp_vis
