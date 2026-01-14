#include "coordinate_system_3d.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include "colors.hpp"
#include "font_atlas.hpp"
#include "gl-handle.hpp"

namespace
{
using namespace explot;
seq_data_desc data_for_coordinate_system(gl_id vbo, std::uint32_t num_ticks)
{
  using v3 = glm::vec3;
  assert(num_ticks > 1);
  auto data = std::vector{v3(-1.0f, -1.0f, -1.0f), v3(1.0f, -1.0f, -1.0f),  v3(-1.0f, -1.0f, -1.0f),
                          v3(-1.0f, 1.0f, -1.0f),  v3(-1.0f, -1.0f, -1.0f), v3(-1.0f, -1.0f, 1.0f)};
  data.reserve(6 + 6 * num_ticks);
  const auto step = 2.0f / (static_cast<float>(num_ticks) - 1.0f);
  const auto radius = 0.02f;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    data.emplace_back(-1.0f + static_cast<float>(i) * step, -1.0f - radius, -1.0f);
    data.emplace_back(-1.0f + static_cast<float>(i) * step, -1.0f + radius, -1.0f);

    data.emplace_back(-1.0f, -1.0f + static_cast<float>(i) * step, -1.0f - radius);
    data.emplace_back(-1.0f, -1.0f + static_cast<float>(i) * step, -1.0f + radius);

    data.emplace_back(-1.0f - radius, -1.0f, -1.0f + static_cast<float>(i) * step);
    data.emplace_back(-1.0f + radius, -1.0f, -1.0f + static_cast<float>(i) * step);
  }
  return data_for_span(vbo, data);
}

} // namespace

namespace explot
{
coordinate_system_3d::coordinate_system_3d(const tics_desc &tics, std::uint32_t num_ticks)
    : scale_to_phase(transform(clip_rect, tics.bounding_rect)), vbo(make_vbo()),
      lines(vbo, data_for_coordinate_system(vbo, num_ticks), 1.0f, axis_color),
      font(make_font_atlas("+-.,0123456789eE"))
{
  const auto &br = tics.bounding_rect;
  const auto steps = (br.upper_bounds - br.lower_bounds) / static_cast<float>(num_ticks - 1);

  xlabels.resize(num_ticks);
  ylabels.resize(num_ticks);
  zlabels.resize(num_ticks);
  for (auto i = 0u; i < num_ticks; ++i)
  {
    const auto p = br.lower_bounds + static_cast<float>(i) * steps;
    update(xlabels[i], format_for_tic(p.x, tics.least_significant_digit_x), font, text_color);
    update(ylabels[i], format_for_tic(p.y, tics.least_significant_digit_y), font, text_color);
    update(zlabels[i], format_for_tic(p.z, tics.least_significant_digit_z), font, text_color);
  }
}

void update(coordinate_system_3d &cs, const transforms_3d &t)
{
  const auto num_ticks = cs.xlabels.size();
  const auto step = 2.0f / static_cast<float>(num_ticks - 1);
  const auto radius = 0.04f;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = t.phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f + static_cast<float>(i) * step, -1.0f - radius, -1.0f, 1.0f);
    offset /= offset.w;
    offset = t.clip_to_screen * offset;
    update(cs.xlabels[i], offset, {0.5f, 1.0f}, t.screen_to_clip);
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = t.phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f, -1.0f + static_cast<float>(i) * step, -1.0f - radius, 1.0f);
    offset /= offset.w;
    offset = t.clip_to_screen * offset;
    update(cs.ylabels[i], offset, {0.5f, 1.0f}, t.screen_to_clip);
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = t.phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f - radius, -1.0f, -1.0f + static_cast<float>(i) * step, 1.0f);
    offset /= offset.w;
    offset = t.clip_to_screen * offset;
    update(cs.zlabels[i], offset, {1.0f, 0.5f}, t.screen_to_clip);
  }

  auto axis_phase_to_clip = t.phase_to_clip * cs.scale_to_phase;
  auto axis_transforms = transforms_3d{.phase_to_clip = axis_phase_to_clip,
                                       .screen_to_clip = t.screen_to_clip,
                                       .clip_to_screen = t.clip_to_screen};
  update(cs.lines, axis_transforms);
}

void draw(const coordinate_system_3d &cs)
{
  draw(cs.lines);
  for (auto &l : cs.xlabels)
  {
    draw(l);
  }
  for (auto &l : cs.ylabels)
  {
    draw(l);
  }
  for (auto &l : cs.zlabels)
  {
    draw(l);
  }
}
} // namespace explot
