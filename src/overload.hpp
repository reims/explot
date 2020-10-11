#pragma once

namespace explot
{
template <typename... Fns>
struct overload final : Fns...
{
  overload(Fns... fns) : Fns(std::move(fns))... {}

  using Fns::operator()...;
};

template <typename... Fns>
overload(Fns...) -> overload<Fns...>;
} // namespace explot
