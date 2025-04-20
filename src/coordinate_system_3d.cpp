#include "coordinate_system_3d.hpp"
#include <vector>
#include <string>
#include <fmt/format.h>
#include "colors.hpp"

namespace
{
using namespace explot;
data_desc data_for_coordinate_system(std::uint32_t num_ticks)
{
  using v3 = glm::vec3;
  assert(num_ticks > 1);
  auto data = std::vector{v3(-1.0f, -1.0f, -1.0f), v3(1.0f, -1.0f, -1.0f),  v3(-1.0f, -1.0f, -1.0f),
                          v3(-1.0f, 1.0f, -1.0f),  v3(-1.0f, -1.0f, -1.0f), v3(-1.0f, -1.0f, 1.0f)};
  data.reserve(6 + 6 * num_ticks);
  const auto step = 2.0f / (static_cast<float>(num_ticks) - 1.0f);
  const auto radius = 0.05f;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    data.emplace_back(-1.0f + i * step, -1.0f - radius, -1.0f);
    data.emplace_back(-1.0f + i * step, -1.0f + radius, -1.0f);

    data.emplace_back(-1.0f, -1.0f + i * step, -1.0f - radius);
    data.emplace_back(-1.0f, -1.0f + i * step, -1.0f + radius);

    data.emplace_back(-1.0f - radius, -1.0f, -1.0f + i * step);
    data.emplace_back(-1.0f + radius, -1.0f, -1.0f + i * step);
  }
  return data_for_span(data);
}

} // namespace

namespace explot
{
coordinate_system_3d::coordinate_system_3d(const tics_desc &tics, std::uint32_t num_ticks)
    : scale_to_phase(transform(clip_rect, tics.bounding_rect)),
      lines(make_lines_state_3d(data_for_coordinate_system(num_ticks))),
      font(make_font_atlas("+-.,0123456789eE", 10).value())
{
  const auto &br = tics.bounding_rect;
  const auto steps = (br.upper_bounds - br.lower_bounds) / static_cast<float>(num_ticks - 1);

  xlabels.reserve(num_ticks);
  ylabels.reserve(num_ticks);
  zlabels.reserve(num_ticks);
  for (auto i = 0u; i < num_ticks; ++i)
  {
    const auto p = br.lower_bounds + static_cast<float>(i) * steps;
    xlabels.emplace_back(font, format_for_tic(p.x, tics.least_significant_digit_x));
    ylabels.emplace_back(font, format_for_tic(p.y, tics.least_significant_digit_y));
    zlabels.emplace_back(font, format_for_tic(p.z, tics.least_significant_digit_z));
  }
}

void draw(const coordinate_system_3d &cs, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip)
{
  draw(cs.lines, 1.0f, phase_to_clip * cs.scale_to_phase, clip_to_screen, screen_to_clip,
       axis_color);
  const auto num_ticks = cs.xlabels.size();
  const auto step = 2.0f / static_cast<float>(num_ticks - 1);
  const auto radius = 0.06f;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f + i * step, -1.0f - radius, -1.0f, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    draw(cs.xlabels[i], screen_to_clip, {offset.x, offset.y}, text_color, {0.5f, 1.0f});
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f, -1.0f + i * step, -1.0f - radius, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    draw(cs.ylabels[i], screen_to_clip, {offset.x, offset.y}, text_color, {0.5f, 1.0f});
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f - radius, -1.0f, -1.0f + i * step, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    draw(cs.zlabels[i], screen_to_clip, {offset.x, offset.y}, text_color, {0.5f, 1.0f});
  }
}
} // namespace explot
