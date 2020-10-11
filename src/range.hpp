#pragma once

#include <variant>

namespace explot
{
struct auto_scale
{
  bool operator==(const auto_scale &) const = default;
};

struct range2d
{
  std::variant<auto_scale, float> min_x;
  std::variant<auto_scale, float> max_x;
  std::variant<auto_scale, float> min_y;
  std::variant<auto_scale, float> max_y;
};
} // namespace explot
