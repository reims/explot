#pragma once

#include "data.hpp"
#include "rect.hpp"

namespace explot
{
rect bounding_rect_2d(const data_desc &d, uint32_t stride);
rect bounding_rect_3d(const data_desc &d, uint32_t stride);
glm::vec2 minmax_x(const data_desc &d, uint32_t stride);
glm::vec2 minmax_y(const data_desc &d, uint32_t stride);
glm::vec2 minmax_z(const data_desc &d, uint32_t stride);
} // namespace explot
