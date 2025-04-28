#include "range_setting.hpp"
#include "overload.hpp"

namespace explot
{
rect apply_view_range(const rect &view, const view_range_2d &range, const rect &auto_scaled)
{
  auto apply_value = [](float view_value, range_value value, float auto_value)
  {
    if (value)
    {
      return std::visit(overload([&](auto_scale) { return auto_value; }, [](float v) { return v; }),
                        value.value());
    }
    else
    {
      return view_value;
    }
  };
  return rect{
      .lower_bounds = glm::vec3(
          apply_value(view.lower_bounds.x, range.x.lower_bound, auto_scaled.lower_bounds.x),
          apply_value(view.lower_bounds.y, range.y.lower_bound, auto_scaled.lower_bounds.y), -1.0f),
      .upper_bounds = glm::vec3(
          apply_value(view.upper_bounds.x, range.x.upper_bound, auto_scaled.upper_bounds.x),
          apply_value(view.upper_bounds.y, range.y.upper_bound, auto_scaled.upper_bounds.y), 1.0f)};
}

view_range_2d merge_ranges(view_range_2d r1, const view_range_2d &r2)
{
  if (r2.x.lower_bound)
  {
    r1.x.lower_bound = r1.x.lower_bound.value_or(r2.x.lower_bound.value());
  }
  if (r2.x.upper_bound)
  {
    r1.x.upper_bound = r1.x.upper_bound.value_or(r2.x.upper_bound.value());
  }
  if (r2.y.lower_bound)
  {
    r1.y.lower_bound = r1.y.lower_bound.value_or(r2.y.lower_bound.value());
  }
  if (r2.y.upper_bound)
  {
    r1.y.upper_bound = r1.y.upper_bound.value_or(r2.y.upper_bound.value());
  }

  return r1;
}
} // namespace explot
