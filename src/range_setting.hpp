#pragma once

#include <variant>
#include <optional>
#include "rect.hpp"
#include <string_view>

namespace explot
{
struct auto_scale
{
};

using range_value = std::optional<std::variant<auto_scale, float>>;

struct range_setting final
{
  range_value lower_bound;
  range_value upper_bound;
};

struct view_range_2d final
{
  range_setting x;
  range_setting y;
};

struct range_match final
{
  range_setting setting;
  std::size_t match;
};

rect apply_view_range(const rect &view, const view_range_2d &range, const rect &auto_scaled);
view_range_2d merge_ranges(view_range_2d r1, const view_range_2d &r2);
std::optional<range_match> match_range(std::string_view str);

} // namespace explot
