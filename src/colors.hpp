#pragma once

#include <glm/vec4.hpp>
#include <type_traits>

namespace explot
{
constexpr glm::vec4 from_rgb(int rgb)
{
  return {static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
          static_cast<float>((rgb >> 8) & 0xFF) / 255.0f, static_cast<float>(rgb & 0xFF) / 255.0f,
          1.0f};
}

inline constexpr glm::vec4 graph_colors[] = {from_rgb(0xa3be8c), from_rgb(0xebcb8b),
                                             from_rgb(0xd08770)};
inline constexpr std::size_t num_graph_colors = std::extent_v<decltype(graph_colors)>;

inline constexpr glm::vec4 background_color = from_rgb(0x4c566a);
inline constexpr glm::vec4 axis_color = from_rgb(0xd8dee9);
inline constexpr glm::vec4 text_color = from_rgb(0x88c0d0);

} // namespace explot
