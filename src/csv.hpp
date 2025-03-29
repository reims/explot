#pragma once
#include <filesystem>
#include <vector>
#include <span>
#include <optional>
#include <chrono>

namespace explot
{
using time_point = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;
std::vector<float> read_csv(const std::filesystem::path &p, char delim, std::span<int> indices,
                            std::optional<time_point> &timebase);

std::uint32_t count_lines(const std::filesystem::path &p);

std::pair<std::vector<float>, unsigned int>
read_matrix_csv(const std::filesystem::path &p, char delim, std::optional<time_point> &timebase);
} // namespace explot
