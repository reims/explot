#pragma once

#include <variant>
#include <string>
#include <vector>
#include <array>
#include "range_setting.hpp"
#include "box.hpp"

namespace explot
{
struct quit_command final
{
};

struct literal_expr final
{
  float value;
};

struct data_ref
{
  int idx;
};

using expr = std::variant<literal_expr, box<struct unary_op>, box<struct binary_op>,
                          box<struct var_or_call>, data_ref>;

enum class unary_operator
{
  minus,
  plus
};

struct unary_op final
{
  unary_operator op;
  expr operand;
};

enum class binary_operator
{
  plus,
  minus,
  mult,
  div
};

struct binary_op final
{
  expr lhs;
  binary_operator op;
  expr rhs;
};

struct var_or_call final
{
  std::string name;
  std::optional<std::vector<expr>> params;
};

struct csv_data_2d final
{
  std::string path;
  std::array<expr, 2> expressions;
};

struct parametric_data_2d final
{
  expr x_expression;
  expr y_expression;
};

enum struct mark_type
{
  points,
  lines
};

using data_source_2d = std::variant<expr, csv_data_2d, parametric_data_2d>;

struct graph_desc_2d final
{
  data_source_2d data;
  mark_type mark;
  std::string title;
};

struct plot_command_2d final
{
  std::vector<graph_desc_2d> graphs;
  range_setting x_range;
  range_setting y_range;
  range_setting t_range;
};

struct csv_data_3d
{
  std::string path;
  std::array<expr, 3> expressions;
};

struct parametric_data_3d final
{
  expr x_expression;
  expr y_expression;
  expr z_expression;
};

using data_source_3d = std::variant<expr, csv_data_3d, parametric_data_3d>;

struct graph_desc_3d final
{
  data_source_3d data;
  mark_type mark;
  std::string title;
};

struct plot_command_3d final
{
  std::vector<graph_desc_3d> graphs;
  range_setting x_range;
  range_setting y_range;
  range_setting u_range;
  range_setting v_range;
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
