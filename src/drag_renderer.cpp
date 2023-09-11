#include "drag_renderer.hpp"
#include "data.hpp"
#include <glm/gtx/string_cast.hpp>
#include "colors.hpp"

namespace explot
{
drag_render_state make_drag_render_state()
{
  float data[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                  0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  auto d = data_for_span(data);
  return drag_render_state{.lines = make_line_strip_state_2d(d), .data = std::move(d)};
}

void draw(const drag_render_state &s, const rect &d, const glm::mat4 &screen_to_clip, float width)
{
  static constexpr auto view =
      rect{.lower_bounds = {0.0f, 0.0f, -1.0f}, .upper_bounds = {1.0f, 1.0f, 1.0f}};
  auto view_to_screen = transform(view, d);
  draw(s.lines, width, view_to_screen, screen_to_clip, selection_color);
}
} // namespace explot
