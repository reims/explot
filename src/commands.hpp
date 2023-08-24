#pragma once

#include <variant>
#include <string>
#include <vector>
#include <array>
#include "range_setting.hpp"

namespace explot
{
struct quit_command final
{
};

struct csv_data_2d final
{
  std::string path;
  std::array<std::string, 2> expressions;
};

struct parametric_data_2d final
{
  std::string x_expression;
  std::string y_expression;
};

enum struct mark_type
{
  points,
  lines
};

using data_source_2d = std::variant<std::string, csv_data_2d, parametric_data_2d>;

struct graph_desc_2d final
{
  data_source_2d data;
  mark_type mark;
};

struct plot_command_2d final
{
  std::vector<graph_desc_2d> graphs;
  range_setting x_range;
  range_setting y_range;
};

struct csv_data_3d
{
  std::string path;
  std::array<std::string, 3> expressions;
};

struct parametric_data_3d final
{
  std::string x_expression;
  std::string y_expression;
  std::string z_expression;
};

using data_source_3d = std::variant<std::string, csv_data_3d, parametric_data_3d>;

struct graph_desc_3d final
{
  data_source_3d data;
  mark_type mark;
};

struct plot_command_3d final
{
  std::vector<graph_desc_3d> graphs;
  range_setting x_range;
  range_setting y_range;
};

struct show_command final
{
  std::vector<std::string> path;
};

struct set_command final
{
  std::vector<std::string> path;
  std::string value;
};

using command =
    std::variant<quit_command, plot_command_2d, plot_command_3d, set_command, show_command>;

inline bool is_quit_command(const command &cmd)
{
  return std::holds_alternative<quit_command>(cmd);
}

inline bool is_plot_command(const command &cmd)
{
  return std::holds_alternative<plot_command_2d>(cmd);
}

inline const plot_command_2d &as_plot_command(const command &cmd)
{
  return std::get<plot_command_2d>(cmd);
}

inline plot_command_2d &as_plot_command(command &cmd) { return std::get<plot_command_2d>(cmd); }
} // namespace explot
