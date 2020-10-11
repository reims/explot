#include "coordinate_system_3d.hpp"
#include "unique_span.hpp"
#include <vector>
#include <string>
#include <fmt/format.h>
#include "colors.hpp"

namespace
{
explot::data_desc data_for_coordinate_system(std::uint32_t num_ticks)
{
  using v3 = glm::vec3;
  assert(num_ticks > 1);
  auto data = explot::make_unique_span<v3>(
      6 + 6 * num_ticks); // 6 points for 3 axis + 2 points per 3 * num_ticks ticks

  data[0] = v3(-1.0f, -1.0f, -1.0f);
  data[1] = v3(1.0f, -1.0f, -1.0f);
  data[2] = v3(-1.0f, -1.0f, -1.0f);
  data[3] = v3(-1.0f, 1.0f, -1.0f);
  data[4] = v3(-1.0f, -1.0f, -1.0f);
  data[5] = v3(-1.0f, -1.0f, 1.0f);

  const auto step = 2.0f / (static_cast<float>(num_ticks) - 1.0f);
  const auto radius = 0.05f;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    data[6 + i * 2] = v3(-1.0f + i * step, -1.0f - radius, -1.0f);
    data[6 + i * 2 + 1] = v3(-1.0f + i * step, -1.0f + radius, -1.0f);

    data[6 + (i + num_ticks) * 2] = v3(-1.0f, -1.0f + i * step, -1.0f - radius);
    data[6 + (i + num_ticks) * 2 + 1] = v3(-1.0f, -1.0f + i * step, -1.0f + radius);

    data[6 + (i + 2 * num_ticks) * 2] = v3(-1.0f - radius, -1.0f, -1.0f + i * step);
    data[6 + (i + 2 * num_ticks) * 2 + 1] = v3(-1.0f + radius, -1.0f, -1.0f + i * step);
  }

  return explot::data_for_span(data);
}

std::vector<std::string> labels_for_ticks(float min, float max, std::uint32_t num_ticks, int digits)
{
  auto result = std::vector<std::string>();
  result.reserve(num_ticks);
  const auto num_intervals = num_ticks - 1;
  // TODO: better way to make sure that truncating yields the string we want
  const auto step = (max - min) / static_cast<float>(num_intervals) * (1.0f + 1.e-8f);
  const auto exp = static_cast<int>(std::ceil(std::log10(step))) - digits;
  const auto precision = exp < 0 ? -exp : 0;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    const auto val = min + static_cast<float>(i) * step;
    result.push_back(fmt::format("{:.{}f}", val, precision));
  }
  return result;
}
} // namespace

namespace explot
{
coordinate_system_3d::coordinate_system_3d(const rect &phase_space, std::uint32_t num_ticks)
    : scale_to_phase(transform(clip_rect, phase_space)),
      data(data_for_coordinate_system(num_ticks)), lines(make_lines_state_3d(data)),
      font(make_font_atlas("+-.,0123456789eE").value()), xlabels(num_ticks), ylabels(num_ticks),
      zlabels(num_ticks)
{
  auto xstrs =
      labels_for_ticks(phase_space.lower_bounds.x, phase_space.upper_bounds.x, num_ticks, 2);
  for (auto i = 0u; i < num_ticks; ++i)
  {
    xlabels[i] = make_gl_string(font, xstrs[i]);
  }
  auto ystrs =
      labels_for_ticks(phase_space.lower_bounds.y, phase_space.upper_bounds.y, num_ticks, 2);
  for (auto i = 0u; i < num_ticks; ++i)
  {
    ylabels[i] = make_gl_string(font, ystrs[i]);
  }
  auto zstrs =
      labels_for_ticks(phase_space.lower_bounds.z, phase_space.upper_bounds.z, num_ticks, 2);
  for (auto i = 0u; i < num_ticks; ++i)
  {
    zlabels[i] = make_gl_string(font, zstrs[i]);
  }
}

void draw(const coordinate_system_3d &cs, const glm::mat4 &phase_to_clip,
          const glm::mat4 &clip_to_screen, const glm::mat4 &screen_to_clip)
{
  draw(cs.lines, 1.0f, phase_to_clip * cs.scale_to_phase, clip_to_screen, screen_to_clip,
       axis_color);
  const auto num_ticks = cs.xlabels.size;
  const auto step = 2.0f / static_cast<float>(num_ticks - 1);
  const auto radius = 0.06f;
  const auto text_scale = 0.4f;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f + i * step, -1.0f - radius, -1.0f, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    draw(cs.xlabels[i], screen_to_clip, {offset.x, offset.y}, text_color, text_scale, {0.5f, 1.0f});
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f, -1.0f + i * step, -1.0f - radius, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    draw(cs.ylabels[i], screen_to_clip, {offset.x, offset.y}, text_color, text_scale, {0.5f, 1.0f});
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f - radius, -1.0f, -1.0f + i * step, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    draw(cs.zlabels[i], screen_to_clip, {offset.x, offset.y}, text_color, text_scale, {0.5f, 1.0f});
  }
}
} // namespace explot
