#include "coordinate_system_3d.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <fmt/format.h>
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
      lines(vbo, data_for_coordinate_system(vbo, num_ticks), 1.0f, axis_color,
            {.phase_to_clip = 6}),
      font(make_font_atlas("+-.,0123456789eE", 10).value()), ubo(make_vbo())
{
  const auto &br = tics.bounding_rect;
  const auto steps = (br.upper_bounds - br.lower_bounds) / static_cast<float>(num_ticks - 1);

  xlabels.reserve(num_ticks);
  ylabels.reserve(num_ticks);
  zlabels.reserve(num_ticks);
  for (auto i = 0u; i < num_ticks; ++i)
  {
    const auto p = br.lower_bounds + static_cast<float>(i) * steps;
    xlabels.emplace_back(font, format_for_tic(p.x, tics.least_significant_digit_x), text_color);
    ylabels.emplace_back(font, format_for_tic(p.y, tics.least_significant_digit_y), text_color);
    zlabels.emplace_back(font, format_for_tic(p.z, tics.least_significant_digit_z), text_color);
  }

  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 6, ubo);
}

void update(const coordinate_system_3d &cs, const glm::mat4 &phase_to_clip,
            const glm::mat4 &clip_to_screen, const glm::mat4 &)
{
  const auto num_ticks = cs.xlabels.size();
  const auto step = 2.0f / static_cast<float>(num_ticks - 1);
  const auto radius = 0.04f;
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f + static_cast<float>(i) * step, -1.0f - radius, -1.0f, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    update(cs.xlabels[i], offset, {0.5f, 1.0f});
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f, -1.0f + static_cast<float>(i) * step, -1.0f - radius, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    update(cs.ylabels[i], offset, {0.5f, 1.0f});
  }
  for (auto i = 0u; i < num_ticks; ++i)
  {
    auto offset = phase_to_clip * cs.scale_to_phase
                  * glm::vec4(-1.0f - radius, -1.0f, -1.0f + static_cast<float>(i) * step, 1.0f);
    offset /= offset.w;
    offset = clip_to_screen * offset;
    update(cs.zlabels[i], offset, {1.0f, 0.5f});
  }

  auto axis_phase_to_clip = phase_to_clip * cs.scale_to_phase;
  glBindBuffer(GL_UNIFORM_BUFFER, cs.ubo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(axis_phase_to_clip));
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
