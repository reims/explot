#pragma once

#include "gl-handle.hpp"
#include "rect.hpp"

namespace explot
{
rect bounding_rect_2d(gl_id vbo, uint32_t num_points);
rect bounding_rect_3d(gl_id vbo, uint32_t num_points);
glm::vec2 minmax_x(gl_id vbo, uint32_t num_points, uint32_t point_size);
glm::vec2 minmax_y(gl_id vbo, uint32_t num_points, uint32_t point_size);
glm::vec2 minmax_z(gl_id vbo, uint32_t num_points, uint32_t point_size);
} // namespace explot
