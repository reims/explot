#pragma once
#include "gl-handle.hpp"
#include <string>
#include <span>
#include <glm/vec3.hpp>
#include "commands.hpp"
#include "range_setting.hpp"

namespace explot
{
struct data_desc final
{
  vbo_handle vbo;
  std::uint32_t num_points;
  std::uint32_t num_segments;
  data_desc() = default;
  data_desc(data_desc &&) = default;
  data_desc(vbo_handle vbo, std::uint32_t num_points, std::uint32_t num_segments = 1);
  data_desc &operator=(data_desc &&) = default;
};

// data_desc data_for_expr(std::string expr, std::size_t num_points);
data_desc data_for_span(std::span<const float> data);
data_desc data_for_span(std::span<const glm::vec3> data);
data_desc data_for_chart_2d(const data_source_2d &src, const plot_command_2d &plot);
data_desc data_for_chart_3d(const data_source_3d &src, const plot_command_3d &plot);
// data_desc data_3d_for_expression(std::string_view expr, std::uint32_t num_lines,
//                                  std::uint32_t num_points_per_line, range_setting xrange = {},
//                                  range_setting yrange = {});
void print_data(const data_desc &data);
} // namespace explot
