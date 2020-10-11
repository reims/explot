#include "framebuffer.hpp"

namespace cpp_vis
{

screen_buffer::screen_buffer(const rect &screen) : color_attachment(make_texture()), fbo(make_fbo())
{
  const auto width = static_cast<std::uint32_t>(screen.upper_bounds.x - screen.lower_bounds.x);
  const auto height = static_cast<std::uint32_t>(screen.lower_bounds.y - screen.lower_bounds.y);
  glBindTexture(GL_TEXTURE_2D, color_attachment);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_attachment, 0);
}
} // namespace cpp_vis
