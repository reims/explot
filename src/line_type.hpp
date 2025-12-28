#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <variant>
#include <optional>
#include <vector>

namespace explot
{

struct dash_type
{
  std::vector<std::pair<uint32_t, uint32_t>> segments;
};

struct solid
{
};

using dash_type_desc = std::variant<solid, dash_type, uint32_t>;

struct line_type final
{
  float width;
  glm::vec4 color;
  std::optional<dash_type> dash_type;
  uint32_t index;
};

} // namespace explot
