#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace explot
{
struct rect
{
  glm::vec3 lower_bounds;
  glm::vec3 upper_bounds;

  bool operator==(const rect &other) const = default;
};

constexpr rect clip_rect = rect{.lower_bounds = glm::vec3(-1.0f, -1.0f, -1.0f),
                                .upper_bounds = glm::vec3(1.0f, 1.0f, 1.0f)};

glm::mat4 transform(const rect &from, const rect &to);
rect scale2d(const rect &r, float s);
rect transform(const rect &r, const glm::mat4 &m);
rect round_for_ticks_2d(const rect &r, int num_ticks, int digits);
rect round_for_ticks_3d(const rect &r, int num_ticks, int digits);
rect union_rect(const rect &r1, const rect &r2);
} // namespace explot
