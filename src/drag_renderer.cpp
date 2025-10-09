#include "drag_renderer.hpp"
#include "data.hpp"
#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "colors.hpp"

namespace
{
using namespace explot;
auto data_for_drag_renderer()
{
  float data[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f};
  return data_for_span(data, 2);
}

} // namespace

namespace explot
{

drag_render_state::drag_render_state(std::tuple<vbo_handle, seq_data_desc> &&t)
    : lines(std::get<0>(t), std::get<1>(t), 1.0f, selection_color), vbo(std::move(std::get<0>(t)))
{
}

drag_render_state::drag_render_state() : drag_render_state(data_for_drag_renderer()) {}

void draw(const drag_render_state &s, const rect &d, const rect &screen)
{
  static constexpr auto view =
      rect{.lower_bounds = {0.0f, 0.0f, -1.0f}, .upper_bounds = {1.0f, 1.0f, 1.0f}};
  auto transforms = transforms_2d{.phase_to_screen = transform(view, d),
                                  .screen_to_clip = transform(screen, clip_rect)};
  update(s.lines, transforms);
  draw(s.lines);
}
} // namespace explot
