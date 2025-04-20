#include "settings.hpp"
#include <fmt/format.h>
#include "overload.hpp"
#include "colors.hpp"
#include <type_traits>

namespace
{
using namespace std::literals;
using namespace explot;
using namespace explot::settings;

template <typename Value>
std::string to_string_(const Value &value)
{
  return fmt::format("{}", value);
}

template <>
std::string to_string_(const data_type &d)
{
  switch (d)
  {
  case data_type::normal:
    return "normal";
  case data_type::time:
    return "time";
  }
}

template <>
std::string to_string_(const samples_setting &setting)
{
  return fmt::format("{},{}", setting.x, setting.y);
}

template <>
std::string to_string_<range_setting>(const range_setting &range)
{
  auto l_str = range.lower_bound ? std::visit(overload([](auto_scale) { return "*"s; },
                                                       [](float x) { return std::to_string(x); }),
                                              range.lower_bound.value())
                                 : ""s;
  auto u_str = range.upper_bound ? std::visit(overload([](auto_scale) { return "*"s; },
                                                       [](float x) { return std::to_string(x); }),
                                              range.upper_bound.value())
                                 : ""s;

  return fmt::format("[{}:{}]", l_str, u_str);
}

static constexpr line_type line_types[] = {line_type{1.f, from_rgb(0xa3be8c)},
                                           line_type{1.f, from_rgb(0xebcb8b)},
                                           line_type{1.f, from_rgb(0xd08770)}};
static constexpr auto num_line_types = std::size(line_types);

template <settings_id id>
settings_type_t<id> default_value()
{
  return {};
}

template <>
char default_value<settings_id::datafile_separator>()
{
  return ',';
}

template <>
std::string default_value<settings_id::timefmt>()
{
  return "%Y-%m-%d %H:%M:%S";
}

template <>
samples_setting default_value<settings_id::samples>()
{
  return {100, 100};
}

template <>
samples_setting default_value<settings_id::isosamples>()
{
  return {10, 10};
}

template <settings_id id>
settings_type_t<id> place = default_value<id>();

} // namespace

namespace explot
{
namespace settings
{
std::string show(const show_command &cmd)
{
  return std::visit([](auto s) { return to_string_(place<decltype(s)::id>); }, cmd.setting);
}

bool set(const set_command &cmd)
{
  std::visit([](const auto &v) { place<std::remove_cvref_t<decltype(v)>::id> = v.value; },
             cmd.value);

  return true;
}

void unset(const unset_command &cmd)
{
  std::visit([](auto v) { place<decltype(v)::id> = default_value<decltype(v)::id>(); },
             cmd.setting);
}

samples_setting samples() { return place<settings_id::samples>; }
samples_setting isosamples() { return place<settings_id::isosamples>; }

bool parametric() { return place<settings_id::parametric>; }
const char *timefmt() { return place<settings_id::timefmt>.c_str(); }
data_type xdata() { return place<settings_id::xdata>; }
const line_type &line_type_by_index(int idx)
{
  assert(idx > 0);
  return line_types[(idx - 1) % num_line_types];
}

namespace datafile
{
char separator() { return place<settings_id::datafile_separator>; }
} // namespace datafile
} // namespace settings
} // namespace explot
