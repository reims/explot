#pragma once

#include <variant>
#include <vector>
#include "box.hpp"
#include "range_setting.hpp"
#include "commands.hpp"

namespace explot
{
namespace ast
{
struct literal_expr final
{
  float value;
};

struct data_ref
{
  int idx;
};

struct unary_op;
struct binary_op;
struct var_or_call;
using expr = std::variant<ast::literal_expr, box<ast::unary_op>, box<ast::binary_op>,
                          box<ast::var_or_call>, ast::data_ref>;

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
  div,
  power
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

struct line_type_spec
{
  std::optional<glm::vec4> color;
  std::optional<float> width;
};

using line_type_desc = std::variant<line_type_spec, uint32_t>;

struct csv_data final
{
  std::string path;
  std::vector<expr> expressions;
  bool matrix;
};

struct parametric_data_2d final
{
  expr x_expression;
  expr y_expression;
};

enum struct mark_type_2d
{
  points,
  lines,
  impulses
};

enum struct mark_type_3d
{
  points,
  lines
};

using data_source_2d = std::variant<expr, csv_data, parametric_data_2d>;

struct graph_desc_2d final
{
  data_source_2d data;
  mark_type_2d mark;
  std::string title;
  line_type_desc line_type;
};

struct plot_command_2d final
{
  std::vector<graph_desc_2d> graphs;
  range_setting x_range;
  range_setting y_range;
  range_setting t_range;
};

struct parametric_data_3d final
{
  expr x_expression;
  expr y_expression;
  expr z_expression;
};

using data_source_3d = std::variant<expr, csv_data, parametric_data_3d>;

struct graph_desc_3d final
{
  data_source_3d data;
  mark_type_3d mark;
  std::string title;
  line_type_desc line_type;
};

struct user_definition
{
  std::string name;
  std::optional<std::vector<std::string>> params;
  expr body;
};

struct plot_command_3d final
{
  std::vector<graph_desc_3d> graphs;
  range_setting x_range;
  range_setting y_range;
  range_setting u_range;
  range_setting v_range;
};
using command = std::variant<plot_command_2d, plot_command_3d, user_definition, set_command,
                             quit_command, unset_command, show_command>;

} // namespace ast

std::optional<ast::command> parse_command_ast(std::string_view line);

std::optional<std::string_view> find_unary_builtin(std::string_view name);
std::optional<std::string_view> find_binary_builtin(std::string_view name);
std::optional<float> find_constant_builtin(std::string_view name);
} // namespace explot
