#include "csv.hpp"

#include <fstream>
#include <string_view>
#include <charconv>
#include <algorithm>
#include <spanstream>
#include "settings.hpp"
#include <date/date.h>

namespace explot
{

std::vector<float> read_csv(const std::filesystem::path &p, char delim, std::span<int> indices,
                            std::optional<time_point> &timebase)
{
  assert(!indices.empty());
  auto result = std::vector<float>();
  auto f = std::ifstream(p);
  auto timefmt = settings::timefmt();
  if (f.is_open())
  {
    char buffer[1 << 10];
    static constexpr auto max_size = std::size(buffer) - 1;
    while (!f.eof())
    {
      f.getline(buffer, max_size);
      auto line = std::string_view(buffer);
      auto it = line.cbegin();
      auto idx = 0u;
      auto csv_index = 1;
      while (it != line.cend() && idx < indices.size())
      {
        auto eof = std::find(it, line.cend(), delim);
        if (csv_index == indices[idx])
        {
          ++idx;
          auto value = 0.0f;
          auto [ptr, ec] = std::from_chars(it, eof, value);
          if (ptr == eof)
          {
            result.push_back(value);
          }
          else
          {
            std::ispanstream ss(std::span(it, eof));
            time_point tp;
            date::from_stream(ss, timefmt, tp);
            if (!ss.fail())
            {
              timebase = timebase.value_or(tp);
              auto d = std::chrono::duration<float>(tp - timebase.value());
              result.push_back(d.count());
            }
            else
            {
              result.push_back(0.0f);
            }
          }
        }
        ++csv_index;
        it = (eof == line.cend() ? eof : eof + 1);
      }
      for (; idx < indices.size(); ++idx)
      {
        result.push_back(0.0f);
      }
    }
  }

  return result;
}

std::uint32_t count_lines(const std::filesystem::path &p)
{
  auto result = 0u;
  auto f = std::ifstream(p);
  if (f.is_open())
  {
    char buffer[1 << 10];
    static constexpr auto max_size = std::size(buffer) - 1;
    while (f.getline(buffer, max_size))
    {
      ++result;
    }
  }
  return result;
}

} // namespace explot
