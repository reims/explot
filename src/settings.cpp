#include "settings.hpp"
#include <fmt/format.h>
#include "commands.hpp"
#include "overload.hpp"
#include "colors.hpp"
#include <tuple>
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
  return "";
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

template <>
std::string to_string_(const std::tuple<int, int, int> &t)
{
  return fmt::format("{},{},{}", std::get<0>(t), std::get<1>(t), std::get<2>(t));
}

template <>
std::string to_string_(const multiplot_setting &m)
{
  return fmt::format("layout {}, {}", m.rows, m.cols);
}
struct predef_line_type
{
  float width;
  glm::vec4 color;
  uint32_t index;
};
static constexpr predef_line_type line_types[] = {{1.f, from_rgb(0xa3be8c), 1},
                                           {1.f, from_rgb(0xebcb8b), 2},
                                           {1.f, from_rgb(0xd08770), 3}};
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

template <>
std::tuple<int, int, int> default_value<settings_id::pallette_rgbformulae>()
{
  return std::make_tuple(7, 5, 15);
}

template <>
multiplot_setting default_value<settings_id::multiplot>()
{
  return {1, 1};
}

template <settings_id id>
settings_type_t<id> place = default_value<id>();

static constexpr std::string_view rgbformulae[] = {"0",
                                                   "0.5",
                                                   "1",
                                                   "x",
                                                   "x*x",
                                                   "x*x*x",
                                                   "x*x*x*x",
                                                   "sqrt(x)",
                                                   "sqrt(sqrt(x))",
                                                   "sin(1.570796 * x)",
                                                   "cos(1.570796 * x)",
                                                   "abs(x-0.5)",
                                                   "(2x-1) * (2x-1)",
                                                   "sin(3.141593 * x)",
                                                   "abs(cos(3.141593 * x))",
                                                   "sin(6.283185 * x)",
                                                   "cos(6.283185 * x)",
                                                   "abs(sin(6.283185 * x))",
                                                   "abs(cos(6.283185 * x))",
                                                   "abs(sin(12.56637 * x))",
                                                   "abs(cos(12.56637 * x))",
                                                   "3 * x",
                                                   "3 * x - 1",
                                                   "3 * x - 2",
                                                   "abs(3 * x - 1)",
                                                   "abs(3 * x - 2)",
                                                   "(3 * x - 1) / 2",
                                                   "(3 * x - 2) / 2",
                                                   "abs(3 * x - 1) / 2",
                                                   "abs(3 * x - 2) / 2",
                                                   "x / 0.32 - 0.78125",
                                                   "2 * x - 0.84",
                                                   "4*x",
                                                   "abs(2 * x - 0.5)",
                                                   "2 * x",
                                                   "2 * x - 0.5",
                                                   "2 * x - 1"};

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
line_type line_type_by_index(uint32_t idx)
{
  assert(idx > 0);
  auto& predef = line_types[(idx - 1) % num_line_types];
  return {.width = predef.width, .color = predef.color, .dash_type = std::nullopt, .index = predef.index};
}
bool hidden3d() { return place<settings_id::hidden3d>; }

std::string_view rgbformula(uint32_t idx)
{
  assert(idx < std::extent_v<decltype(rgbformulae)>);
  return rgbformulae[idx];
}

const multiplot_setting &multiplot() { return place<settings_id::multiplot>; }

namespace datafile
{
char separator() { return place<settings_id::datafile_separator>; }
} // namespace datafile

namespace palette
{
std::tuple<int, int, int> rgbformulae() { return place<settings_id::pallette_rgbformulae>; }
} // namespace palette
} // namespace settings
} // namespace explot
