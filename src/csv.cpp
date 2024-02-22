#include "csv.hpp"

#include <fstream>
#include <string_view>
#include <charconv>
#include <algorithm>

namespace explot
{

std::vector<float> read_csv(const std::filesystem::path &p, char delim, std::span<int> indices)
{
  assert(!indices.empty() || indices.front() == 2);
  auto result = std::vector<float>();

  auto f = std::ifstream(p);
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
          auto [_, ec] = std::from_chars(it, eof, value);
          result.push_back(value);
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
