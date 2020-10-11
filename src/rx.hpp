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

auto scale(auto set, auto multiply, auto divide)
{
  enum class op_type
  {
    set,
    multiply,
    divide
  };

  struct op
  {
    op_type op;
    float value;
  };

  auto ops = rx::observable<>::from(set
                                        | rx::map(
                                            [](float v) {
                                              return op{op_type::set, v};
                                            })
                                        | rx::as_dynamic(),
                                    multiply
                                        | rx::map(
                                            [](float v) {
                                              return op{op_type::multiply, v};
                                            }),
                                    divide
                                        | rx::map(
                                            [](float v) {
                                              return op{op_type::divide, v};
                                            }))
                 .merge()
             | rx::tap([](auto) { std::cout << "new op\n"; });
  return ops
         | rx::scan(1.0f,
                    [](float v, op o)
                    {
                      std::cout << "new op with value " << o.value << '\n';
                      switch (o.op)
                      {
                      case op_type::set:
                        return o.value;
                      case op_type::multiply:
                        return v * o.value;
                      case op_type::divide:
                        return v / o.value;
                      }
                      return 0.0f; // should not be reached
                    })
         | rx::start_with(1.0f) | rx::tap([](float) { std::cout << "got scale\n"; });
}

} // namespace explot
