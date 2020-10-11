#pragma once

#include "rx.hpp"
#include <glm/vec2.hpp>
#include <GLFW/glfw3.h>
#include "rect.hpp"

namespace explot
{
void init_events(GLFWwindow *window);

rx::observable<glm::vec2> mouse_moves();
rx::observable<int> mouse_ups();
rx::observable<int> mouse_downs();
rx::observable<int> key_presses();

struct drag
{
  glm::vec2 from;
  glm::vec2 to;
};

rx::observable<rx::observable<drag>> drags();
rx::observable<drag> drops();

rect drag_to_rect(const drag &d, float width);
} // namespace explot
