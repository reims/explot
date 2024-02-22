#include "impulse.hpp"

namespace explot
{
void draw(const impulses_state &s, float width, const glm::mat4 &phase_to_screen,
          const glm::mat4 &screen_to_clip, const glm::vec4 &color)
{
  draw(s.lines, width, phase_to_screen, screen_to_clip, color);
}

} // namespace explot
