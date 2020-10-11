#include "parse_commands.hpp"
#include <ctre.hpp>
#include <string_view>
#include <charconv>
#include <cassert>
#include <fmt/format.h>

namespace
{
std::vector<std::string> split_string(std::string_view s)
{
  auto result = std::vector<std::string>();
  while (!s.empty())
  {
    auto p = s.find(' ');
    if (p == s.npos)
    {
      p = s.size();
    }
    if (p > 0)
    {
      result.emplace_back(s.substr(0, p));
    }
    s.remove_prefix(std::min(s.size(), p + 1));
  }
  return result;
}

static constexpr char path_regex[] = "\\s*\"([a-zA-Z\\-\\._/]+)\"\\s*";
static constexpr char kw_regex[] = "with|using|,";
static constexpr char using_2d_regex[] = "using ((\\([^:]+)|([0-9]+)):((\\([^:]+)|([0-9]+))\\s*";
static constexpr char using_3d_regex[] =
    "using ((\\([^:]+)|([0-9]+)):((\\([^:]+)|([0-9]+)):((\\([^:]+)|([0-9]+))\\s*";
static constexpr char with_regex[] = "with (lines|points)\\s*";
static constexpr char settings_path_regex[] = "(\\s+[a-zA-z]+)+";

template <typename T>
struct match final
{
  std::size_t matched;
  T value;
  match(std::size_t matched, T value) : matched(matched), value(std::move(value)) {}
};

match<explot::data_source_2d> match_data_2d(std::string_view params)
{
  using namespace explot;
  if (auto [whole, path] = ctre::starts_with<path_regex>(params); whole)
  {
    return match(whole.size(), data_source_2d(csv_data_2d{.path = std::string(path),
                                                          .expressions = {"$0", "$1"}}));
  }
  else
  {
    auto [kw] = ctre::search<kw_regex>(params);
    auto end = kw ? kw.begin() : params.end();
    return match(static_cast<std::size_t>(std::distance(params.begin(), end)),
                 data_source_2d(std::string(params.begin(), end)));
  }
}

match<explot::data_source_3d> match_data_3d(std::string_view params)
{
  using namespace explot;
  if (auto [whole, path] = ctre::starts_with<path_regex>(params); whole)
  {
    return match(whole.size(), data_source_3d(csv_data_3d{.path = std::string(path),
                                                          .expressions = {"$0", "$1", "$2"}}));
  }
  else
  {
    auto [kw] = ctre::search<kw_regex>(params);
    auto end = kw ? kw.begin() : params.end();
    return match(static_cast<std::size_t>(std::distance(params.begin(), end)),
                 data_source_3d(std::string(params.begin(), end)));
  }
}

std::size_t match_using_2d(std::string_view params, explot::csv_data_2d &csv)
{
  if (auto [whole, x_str, x_expr, x_idx, y_str, y_expr, y_idx] =
          ctre::starts_with<using_2d_regex>(params);
      whole)
  {
    if (x_expr)
    {
      csv.expressions[0] = x_expr.view().substr(1, x_expr.size() - 2);
    }
    else if (x_idx)
    {
      csv.expressions[0] = fmt::format("${}", x_idx);
    }
    if (y_expr)
    {
      csv.expressions[1] = y_expr.view().substr(1, y_expr.size() - 2);
    }
    else if (y_idx)
    {
      csv.expressions[1] = fmt::format("${}", y_idx);
    }
    return whole.size();
  }
  return 0U;
}

std::size_t match_using_3d(std::string_view params, explot::csv_data_3d &csv)
{
  if (auto [whole, x_str, x_expr, x_idx, y_str, y_expr, y_idx, z_str, z_expr, z_idx] =
          ctre::starts_with<using_3d_regex>(params);
      whole)
  {
    if (x_expr)
    {
      csv.expressions[0] = x_expr.view().substr(1, x_expr.size() - 2);
    }
    else if (x_idx)
    {
      csv.expressions[0] = fmt::format("${}", x_idx);
    }
    if (y_expr)
    {
      csv.expressions[1] = y_expr.view().substr(1, y_expr.size() - 2);
    }
    else if (y_idx)
    {
      csv.expressions[1] = fmt::format("${}", y_idx);
    }
    if (z_expr)
    {
      csv.expressions[2] = z_expr.view().substr(1, z_expr.size() - 2);
    }
    else if (z_idx)
    {
      csv.expressions[2] = fmt::format("${}", z_idx);
    }
    return whole.size();
  }
  return 0U;
}

std::optional<match<explot::mark_type>> match_mark(std::string_view params)
{
  using namespace explot;
  if (auto [whole, mark_str] = ctre::starts_with<with_regex>(params); whole)
  {
    if (mark_str == "lines")
    {
      return match(whole.size(), mark_type::lines);
    }
    else if (mark_str == "points")
    {
      return match(whole.size(), mark_type::points);
    }
    else
    {
      return std::nullopt;
    }
  }
  return std::nullopt;
}
} // namespace

namespace explot
{
std::optional<command> parse_command(const char *cmd)
{
  auto line = std::string_view(cmd);
  if (line.starts_with("quit"))
  {
    return quit_command();
  }
  else if (line.starts_with("show "))
  {
    if (auto [whole, p] = ctre::starts_with<settings_path_regex>(line.substr(4)); whole)
    {
      return show_command{.path = split_string(whole)};
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (line.starts_with("set "))
  {
    if (auto [whole, p] = ctre::starts_with<settings_path_regex>(line.substr(3)); whole)
    {
      auto value = line.substr(3 + whole.size());
      while (!value.empty() && value[0] == ' ')
      {
        value.remove_prefix(1);
      }
      fmt::print("set '{}' '{}'\n", whole, value);
      return set_command{.path = split_string(whole), .value = std::string(value)};
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (line.starts_with("plot "))
  {
    auto result = plot_command_2d();
    auto params = line.substr(5);
    while (!params.empty())
    {
      if (params[0] == ',')
      {
        params = params.substr(1);
      }
      auto [matched, data] = match_data_2d(params);
      params = params.substr(matched);
      auto mark = mark_type::points;
      while (!params.empty() && params[0] != ',')
      {
        if (std::holds_alternative<csv_data_2d>(data))
        {
          if (auto matched = match_using_2d(params, std::get<csv_data_2d>(data)); matched > 0)
          {
            params = params.substr(matched);
            continue;
          }
        }
        if (auto mark_match = match_mark(params); mark_match)
        {
          auto [matched, new_mark] = mark_match.value();
          mark = new_mark;
          params = params.substr(matched);
        }
        else
        {
          return std::nullopt;
        }
      }
      result.graphs.push_back({data, mark});
    }
    return result;
  }
  else if (line.starts_with("splot"))
  {
    auto result = plot_command_3d();
    auto params = line.substr(6);
    if (auto x_range_match = match_range(params); x_range_match)
    {
      result.x_range = x_range_match.value().setting;
      params = params.substr(x_range_match.value().match);
      if (auto y_range_match = match_range(params); y_range_match)
      {
        result.y_range = y_range_match.value().setting;
        params = params.substr(y_range_match.value().match);
      }
    }
    while (!params.empty())
    {
      if (params[0] == ',')
      {
        params = params.substr(1);
      }
      auto [matched, data] = match_data_3d(params);
      params = params.substr(matched);
      auto mark = mark_type::points;
      while (!params.empty() && params[0] != ',')
      {
        if (std::holds_alternative<csv_data_3d>(data))
        {
          if (auto matched = match_using_3d(params, std::get<csv_data_3d>(data)); matched > 0)
          {
            params = params.substr(matched);
            continue;
          }
        }
        if (auto mark_match = match_mark(params); mark_match)
        {
          auto [matched, new_mark] = mark_match.value();
          mark = new_mark;
          params = params.substr(matched);
        }
        else
        {
          return std::nullopt;
        }
      }
      result.graphs.push_back({data, mark});
    }
    return result;
  }
  else
  {
    return std::nullopt;
  }
}
} // namespace explot
