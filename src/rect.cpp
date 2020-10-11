#include "rect.hpp"
#include <glm/ext/matrix_transform.hpp>
// #include <glm/gtx/string_cast.hpp>
// #include <iostream>
#include <algorithm>

namespace
{
struct interval final
{
  float min;
  float max;
};

auto round_to_ticks(float min, float max, int num_ticks, int digits)
{
  assert(max > min);
  assert(num_ticks > 1);
  const auto num_splits = static_cast<float>(num_ticks - 1);
  const auto raw_interval = (max - min) / num_splits;
  const auto exp = static_cast<int>(std::ceil(std::log10(raw_interval))) - digits;
  const auto resolution = [&]()
  {
    auto result = 1.0f;
    if (exp > 0)
    {
      for (int i = 0; i < exp; ++i)
      {
        result *= 10.0f;
      }
    }
    else
    {
      for (int i = exp; i < 0; ++i)
      {
        result /= 10.0f;
      }
    }
    return result;
  }();
  const auto start = std::floor(min / resolution + 1.e-6f) * resolution;
  const auto width =
      std::ceil((max - start) / (num_splits * resolution) + 1.e-6f) * num_splits * resolution;
  return interval{.min = start, .max = start + width};
}

} // namespace

namespace explot
{
glm::mat4 transform(const rect &from, const rect &to)
{
  auto s = (to.upper_bounds - to.lower_bounds) / (from.upper_bounds - from.lower_bounds);
  // std::cout << glm::to_string(s) << '\n';
  auto v = to.lower_bounds - s * from.lower_bounds;
  // std::cout << glm::to_string(v) << '\n';
  return glm::translate(glm::mat4(1.0f), v) * glm::scale(glm::mat4(1.0f), s);
}

rect scale2d(const rect &r, float s)
{
  auto mid = (r.lower_bounds + r.upper_bounds) / 2.0f;
  auto diag = s * (r.upper_bounds - mid);
  return {.lower_bounds = mid - diag, .upper_bounds = mid + diag};
}

rect transform(const rect &r, const glm::mat4 &m)
{
  return {.lower_bounds = m * glm::vec4(r.lower_bounds, 1.0f),
          .upper_bounds = m * glm::vec4(r.upper_bounds, 1.0f)};
}

rect round_for_ticks_2d(const rect &r, int num_ticks, int digits)
{
  const auto interval_x = round_to_ticks(r.lower_bounds.x, r.upper_bounds.x, num_ticks, digits);
  const auto interval_y = round_to_ticks(r.lower_bounds.y, r.upper_bounds.y, num_ticks, digits);
  return {.lower_bounds = {interval_x.min, interval_y.min, r.lower_bounds.z},
          .upper_bounds = {interval_x.max, interval_y.max, r.upper_bounds.z}};
}

rect round_for_ticks_3d(const rect &r, int num_ticks, int digits)
{
  const auto interval_x = round_to_ticks(r.lower_bounds.x, r.upper_bounds.x, num_ticks, digits);
  const auto interval_y = round_to_ticks(r.lower_bounds.y, r.upper_bounds.y, num_ticks, digits);
  const auto interval_z = round_to_ticks(r.lower_bounds.z, r.upper_bounds.z, num_ticks, digits);
  return {.lower_bounds = {interval_x.min, interval_y.min, interval_z.min},
          .upper_bounds = {interval_x.max, interval_y.max, interval_z.max}};
}

rect union_rect(const rect &r1, const rect &r2)
{
  return rect{.lower_bounds = {std::min(r1.lower_bounds.x, r2.lower_bounds.x),
                               std::min(r1.lower_bounds.y, r2.lower_bounds.y),
                               std::min(r1.lower_bounds.z, r2.lower_bounds.z)},
              .upper_bounds = {std::max(r1.upper_bounds.x, r2.upper_bounds.x),
                               std::max(r1.upper_bounds.y, r2.upper_bounds.y),
                               std::max(r1.upper_bounds.z, r2.upper_bounds.z)}};
}
} // namespace explot
