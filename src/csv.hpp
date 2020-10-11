#pragma once
#include <filesystem>
#include <vector>
#include <glm/vec3.hpp>
#include <span>

namespace explot
{
std::vector<glm::vec3> read_csv(const std::filesystem::path &p, char delim, int x_index,
                                int y_index, int z_index = 0);
std::vector<float> read_csv(const std::filesystem::path &p, char delim, std::span<int> indices);
} // namespace explot
