#pragma once
#include "gl-handle.hpp"
#include <cstdint>
#include <span>
#include <glm/vec3.hpp>
#include "commands.hpp"
#include <tuple>
#include <variant>
#include <vector>
#include "csv.hpp"

namespace explot
{
struct seq_data_desc
{
  uint32_t num_points;
  uint32_t point_size;
  std::vector<GLsizei> count;

  seq_data_desc() = default;
  seq_data_desc(uint32_t point_size, uint32_t num_points, uint32_t num_segments = 1);
  seq_data_desc(uint32_t point_size, std::vector<GLsizei> count);
};

struct grid_data_desc
{
  uint32_t num_rows;
  uint32_t num_columns;
  uint32_t point_size;
};

using data_desc = std::variant<seq_data_desc, grid_data_desc>;

struct draw_info
{
  draw_info() = default;
  draw_info(vbo_handle ebo, std::vector<GLsizei> count);

  vbo_handle ebo;
  uint32_t num_indices;
  std::vector<GLsizei> count;
  std::vector<intptr_t> starts;
};

std::tuple<std::vector<std::tuple<vbo_handle, seq_data_desc>>, time_point>
data_for_plot(const plot_command_2d &plot);

std::vector<std::tuple<vbo_handle, data_desc>> data_for_plot(const plot_command_3d &plot);

std::tuple<vbo_handle, seq_data_desc> data_for_span(std::span<const float> data,
                                                    uint32_t point_size);

std::tuple<vbo_handle, seq_data_desc> data_for_span(std::span<const glm::vec3> data);

seq_data_desc data_for_span(gl_id, std::span<const glm::vec3> data);

seq_data_desc data_for_span(gl_id vbo, std::span<const float> data, uint32_t point_size);

seq_data_desc reshape(seq_data_desc d, std::uint32_t new_point_size);

draw_info sequential_draw_info(const seq_data_desc &d);

draw_info sequential_draw_info(const grid_data_desc &d);

draw_info grid_lines_draw_info(const grid_data_desc &d);

draw_info surface_draw_info(const grid_data_desc &d);
} // namespace explot
