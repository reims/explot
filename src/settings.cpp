#include "settings.hpp"
// #include <mutex>
#include <fmt/format.h>
#include <charconv>
#include <memory>
#include <optional>
#include <ctre.hpp>
#include "overload.hpp"
#include "parse_commands.hpp"

namespace
{
using namespace std::literals;
using namespace explot;
using namespace explot::settings;
struct settings_t final
{
  samples_setting samples = {100, 100};
  samples_setting isosamples = {10, 10};
  struct
  {
    char separator = ',';
  } datafile;
  view_range_2d range = {};
  bool parametric = false;
} settings_v;

template <typename Value>
std::string to_string_(const Value &value)
{
  return fmt::format("{}", value);
}
template <typename Value>
bool set_(Value &value, std::string_view str)
{
  auto [_, errc] = std::from_chars(str.begin(), str.end(), value);
  return errc == std::errc();
}

template <>
bool set_(bool &value, std::string_view str)
{
  value = true;
  return true;
}

template <>
bool set_<samples_setting>(samples_setting &setting, std::string_view s)
{
  static constexpr const char re[] = R"re(\s*(\d+)(\s*,\s*(\d+))?)re";
  if (auto [whole, x_str, _, y_str] = ctre::starts_with<re>(s); whole)
  {
    std::size_t x = 0;
    auto [x_ptr, x_errc] = std::from_chars(x_str.begin(), x_str.end(), x);
    if (x_errc != std::errc() || x < 2)
    {
      return false;
    }
    std::size_t y = x;
    if (y_str)
    {
      auto [y_ptr, y_errc] = std::from_chars(y_str.begin(), y_str.end(), y);
      if (y_errc != std::errc() || y < 2)
      {
        return false;
      }
    }
    setting.x = x;
    setting.y = y;
    return true;
  }
  return false;
}

template <>
std::string to_string_(const samples_setting &setting)
{
  return fmt::format("{},{}", setting.x, setting.y);
}

template <>
bool set_<char>(char &value, std::string_view s)
{
  if (s.size() < 3 || s[0] != '"' || s[2] != '"')
  {
    return false;
  }
  else
  {
    value = s[1];
    return true;
  }
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
bool set_<range_setting>(range_setting &range, std::string_view str)
{
  auto match = parse_range_setting(str);
  if (match)
  {
    range = match.value();
    return true;
  }
  else
  {
    return false;
  }
}

struct settings_value final
{
  struct impl_base
  {
    virtual std::string to_string() const = 0;
    virtual bool set(std::string_view value) = 0;
    virtual ~impl_base() = default;
  };

  template <typename Value>
  struct impl_derived final : impl_base
  {
    Value &value;

    impl_derived(Value &value) : value(value) {}

    std::string to_string() const override { return to_string_(value); }
    bool set(std::string_view str) override { return set_(value, str); }
  };

  std::unique_ptr<impl_base> impl;
  template <typename Value>
  explicit settings_value(Value &value) : impl(std::make_unique<impl_derived<Value>>(value))
  {
  }

  std::string to_string() const { return impl->to_string(); }

  bool set(std::string_view v) { return impl->set(v); }
};

std::optional<settings_value> get_value(std::span<const std::string> path)
{
  if (path.size() == 1 && path[0] == "samples")
  {
    return settings_value(settings_v.samples);
  }
  else if (path.size() == 1 && path[0] == "isosamples")
  {
    return settings_value(settings_v.isosamples);
  }
  else if (path.size() == 2 && path[0] == "datafile")
  {
    if (path[1] == "separator")
    {
      return settings_value(settings_v.datafile.separator);
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (path.size() == 1 && path[0] == "xrange")
  {
    return settings_value(settings_v.range.x);
  }
  else if (path.size() == 1 && path[0] == "yrange")
  {
    return settings_value(settings_v.range.y);
  }
  else if (path.size() == 1 && path[0] == "parametric")
  {
    return settings_value(settings_v.parametric);
  }
  else
  {
    return std::nullopt;
  }
}
} // namespace

namespace explot
{
namespace settings
{
std::string show(std::span<const std::string> path)
{
  auto v = get_value(path);
  if (v)
  {
    return v.value().to_string();
  }
  else
  {
    return "did not find value";
  }
}

bool set(std::span<const std::string> path, std::string_view value)
{
  auto v = get_value(path);
  if (v)
  {
    return v.value().set(value);
  }
  else
  {
    return false;
  }
}

samples_setting samples() { return settings_v.samples; }
samples_setting isosamples() { return settings_v.isosamples; }

bool parametric() { return settings_v.parametric; }

namespace datafile
{
char separator() { return settings_v.datafile.separator; }
} // namespace datafile
} // namespace settings
} // namespace explot
