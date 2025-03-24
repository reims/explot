#pragma once
#include "gl-handle.hpp"
#include <span>
#include <glm/vec3.hpp>
#include "commands.hpp"
#include <utility>
#include "csv.hpp"

namespace explot
{
struct data_desc final
{
  vbo_handle vbo;
  vbo_handle ebo;
  std::uint32_t num_points;
  std::uint32_t point_size;
  std::vector<uintptr_t> starts;
  std::vector<GLsizei> count;
  data_desc() = default;
  data_desc(data_desc &&) = default;
  data_desc(vbo_handle vbo, std::uint32_t point_size, std::uint32_t num_points,
            std::uint32_t num_segments = 1);
  data_desc(vbo_handle vbo, std::uint32_t point_size, std::vector<GLsizei> count);
  data_desc &operator=(data_desc &&) = default;
};

std::pair<std::vector<data_desc>, time_point> data_for_plot(const plot_command_2d &plot);
std::vector<data_desc> data_for_plot(const plot_command_3d &plot);
data_desc data_for_span(std::span<const float> data, size_t stride);
data_desc data_for_span(std::span<const glm::vec3> data);
data_desc reshape(data_desc d, std::uint32_t new_point_size);

void print_data(const data_desc &data);
} // namespace explot
