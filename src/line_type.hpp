#pragma once

#include "commands.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <span>
#include <vector>

namespace explot
{
struct line_type final
{
  float width;
  glm::vec4 color;
  std::optional<dash_type> dash_type;
  uint32_t index;
};

std::vector<line_type> resolve_line_types(std::span<const graph_desc_2d> graphs);
std::vector<line_type> resolve_line_types(std::span<const graph_desc_3d> graphs);
} // namespace explot
