#include "csv.hpp"

#include <fstream>
#include <charconv>
#include <algorithm>
#include <spanstream>
#include "settings.hpp"
#include <date/date.h>
#include <memory>
#include <cstring>
#include <cctype>

namespace
{
using namespace explot;

float parse_field(const char *s, const char *e, std::optional<time_point> &timebase)
{
  auto value = 0.0f;
  auto [ptr, ec] = std::from_chars(s, e, value);
  if (ptr == e)
  {
    return value;
  }
  else
  {
    std::ispanstream ss(std::span(s, e));
    time_point tp;
    date::from_stream(ss, settings::timefmt(), tp);
    if (!ss.fail())
    {
      timebase = timebase.value_or(tp);
      auto d = std::chrono::duration<float>(tp - timebase.value());
      return d.count();
    }
    else
    {
      return 0.0f;
    }
  }
}

void read_csv_impl(std::ifstream &f, char delim, auto handle_field, auto handle_end_of_line)
{
  static constexpr auto buffer_size = 1z << 16;
  if (f.is_open())
  {
    auto buffer = std::make_unique_for_overwrite<char[]>(buffer_size);
    auto offset = 0z;
    while (!(f.eof() || f.bad()))
    {
      f.read(buffer.get() + offset, buffer_size - offset - 1z);
      auto read = f.gcount();
      auto c = buffer.get();
      auto start_of_field = c;
      auto end = c + read;
      for (; c != end; ++c)
      {
        if (*c == delim)
        {
          handle_field(start_of_field, c);
          start_of_field = c + 1;
        }
        else if (*c == '\n')
        {
          if (!std::all_of(start_of_field, c, [](char c) { return std::isspace(c); }))
          {
            handle_field(start_of_field, c);
          }
          start_of_field = c + 1;
          handle_end_of_line();
        }
      }
      offset = c - start_of_field;
      std::memmove(buffer.get(), start_of_field, static_cast<std::size_t>(offset));
    }
  }
}

} // namespace

namespace explot
{

std::vector<float> read_csv(const std::filesystem::path &p, char delim, std::span<int> indices,
                            std::optional<time_point> &timebase)
{
  auto result = std::vector<float>();

  auto f = std::ifstream(p, std::ios::binary);

  auto idx = 0uz;
  auto csv_idx = 0;
  read_csv_impl(
      f, delim,
      [&](const char *s, const char *e)
      {
        ++csv_idx; // this is 1-based
        if (idx < indices.size() && csv_idx == indices[idx])
        {
          result.push_back(parse_field(s, e, timebase));
          ++idx;
        }
      },
      [&]
      {
        for (; idx < indices.size(); ++idx)
        {
          result.push_back(0.0f);
        }
        idx = 0uz;
        csv_idx = 0;
      });

  return result;
}

std::uint32_t count_lines(const std::filesystem::path &p)
{
  auto result = 0u;
  auto f = std::ifstream(p);
  if (f.is_open())
  {
    static constexpr auto buffer_size = 1z << 16;
    auto buffer = std::make_unique_for_overwrite<char[]>(buffer_size);
    while (!f.eof() && !f.bad())
    {
      std::memset(buffer.get(), 0, static_cast<size_t>(buffer_size));
      f.read(buffer.get(), buffer_size - 1);
      result += std::count(buffer.get(), buffer.get() + buffer_size, '\n');
    }
  }
  return result;
}

std::pair<std::vector<float>, unsigned int>
read_matrix_csv(const std::filesystem::path &p, char delim, std::optional<time_point> &timebase)
{
  auto result = std::vector<float>();

  auto f = std::ifstream(p, std::ios::binary);
  auto columns = std::optional<unsigned int>();
  auto column = 0u;
  auto row = 0u;
  read_csv_impl(
      f, delim,
      [&](const char *s, const char *e)
      {
        if (!columns.has_value() || *columns > column)
        {
          result.emplace_back(column);
          result.emplace_back(row);
          ++column;
          result.push_back(parse_field(s, e, timebase));
        }
      },
      [&]
      {
        columns = columns.value_or(column);
        if (column < *columns)
        {
          std::fill_n(std::back_inserter(result), *columns - column, 0.0f);
        }
        column = 0;
        row++;
      });

  return {result, columns.value_or(0)};
}

} // namespace explot
