#pragma once

#include <rxcpp/rx.hpp>
#include <variant>

namespace rx
{
using namespace rxcpp;
using namespace rxcpp::sources;
using namespace rxcpp::operators;
using namespace rxcpp::util;
} // namespace rx

namespace explot
{
using unit = std::monostate;

inline auto to_unit()
{
  return rx::transform([](auto) { return unit{}; });
}

auto closed(auto obs)
{
  return rx::observable<>::create<unit>(
      [=](rxcpp::subscriber<unit> s)
      {
        return obs.subscribe([](auto &&) {}, [=](rx::error_ptr e) { s.on_error(e); },
                             [=]()
                             {
                               s.on_next(unit{});
                               s.on_completed();
                             });
      });
}

} // namespace explot
