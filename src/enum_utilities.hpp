#pragma once

#include <utility>
#include <variant>

namespace explot
{
template <typename E, E... es>
struct enum_sequence
{
};

template <typename E, template <E> typename F, typename Es>
struct enum_sum;

template <typename E, template <E> typename F, E... Es>
struct enum_sum<E, F, enum_sequence<E, Es...>>
{
  using type = std::variant<F<Es>...>;
};

template <typename E, template <E> typename F, typename Es>
using enum_sum_t = typename enum_sum<E, F, Es>::type;

} // namespace explot
