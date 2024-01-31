#pragma once
#include <filesystem>
#include <vector>
#include <glm/vec3.hpp>
#include <span>

namespace explot
{
std::vector<float> read_csv(const std::filesystem::path &p, char delim, std::span<int> indices);
} // namespace explot
