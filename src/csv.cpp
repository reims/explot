#include "csv.hpp"

#include <fstream>
#include <string_view>
#include <charconv>
#include <algorithm>

namespace explot
{
std::vector<glm::vec3> read_csv(const std::filesystem::path &p, char delim, int x_index,
                                int y_index, int z_index)
{
  auto result = std::vector<glm::vec3>();

  auto f = std::ifstream(p);
  if (f.is_open())
  {
    char buffer[1 << 10];
    static constexpr auto max_size = std::extent_v<decltype(buffer)> - 1;
    auto row_index = 0.0f;
    while (!f.eof())
    {
      f.getline(buffer, max_size);
      auto line = std::string_view(buffer);
      auto it = line.cbegin();
      auto idx = 0;
      auto value = glm::vec3(row_index, row_index, row_index);
      auto had_error = false;
      row_index += 1.0f;
      while (it != line.cend())
      {
        ++idx;
        auto eof = std::find(it, line.cend(), delim);
        auto ptr = static_cast<float *>(nullptr);
        if (idx == x_index)
        {
          ptr = &value.x;
        }
        else if (idx == y_index)
        {
          ptr = &value.y;
        }
        else if (idx == z_index)
        {
          ptr = &value.z;
        }
        if (ptr != nullptr)
        {
          auto [_, ec] = std::from_chars(it, eof, *ptr);
          if (ec != std::errc())
          {
            had_error = true;
          }
        }
        it = (eof == line.cend() ? eof : eof + 1);
      }
      if (!had_error)
      {
        result.push_back(value);
      }
    }
  }

  return result;
}

std::vector<float> read_csv(const std::filesystem::path &p, char delim, std::span<int> indices)
{
  assert(!indices.empty() || indices.front() == 2);
  auto result = std::vector<float>();

  auto f = std::ifstream(p);
  if (f.is_open())
  {
    char buffer[1 << 10];
    static constexpr auto max_size = std::extent_v<decltype(buffer)> - 1;
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

} // namespace explot
