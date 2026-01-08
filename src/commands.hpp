#pragma once

#include <variant>
#include <string>
#include <vector>
#include <filesystem>
#include "range_setting.hpp"
#include "box.hpp"
#include "enum_utilities.hpp"
#include "line_type.hpp"

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

struct var
{
  std::string name;
};

struct user_var_ref
{
  uint32_t idx;
};

using expr = std::variant<literal_expr, box<struct unary_op>, box<struct binary_op>,
                          box<struct unary_builtin_call>, box<struct binary_builtin_call>,
                          box<struct user_function_call>, user_var_ref, var, data_ref>;

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

struct unary_builtin_call
{
  std::string name;
  expr arg;
};

struct binary_builtin_call
{
  std::string name;
  expr arg1;
  expr arg2;
};

struct user_function_call
{
  uint32_t idx;
  std::vector<expr> args;
};

struct csv_data final
{
  std::string path;
  std::vector<expr> expressions;
  bool matrix;
};

struct parametric_data_2d final
{
  expr expressions[2];
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
  lines,
  surface,
  pm3d
};

using data_source_2d = std::variant<expr, csv_data, parametric_data_2d>;

struct graph_desc_2d final
{
  data_source_2d data;
  mark_type_2d mark;
  std::string title;
  line_type line_type;
};

enum class data_type : char
{
  normal,
  time
};

struct samples_setting final
{
  uint32_t x = 100;
  uint32_t y = 100;
};

struct plot_command_2d final
{
  std::vector<graph_desc_2d> graphs;
  range_setting x_range;
  range_setting y_range;
  range_setting t_range;
  data_type xdata;
  samples_setting samples;
  samples_setting isosamples;
  char separator;
};

struct parametric_data_3d final
{
  expr expressions[3];
};

using data_source_3d = std::variant<expr, csv_data, parametric_data_3d>;

struct graph_desc_3d final
{
  data_source_3d data;
  mark_type_3d mark;
  std::string title;
  line_type line_type;
};

struct plot_command_3d final
{
  std::vector<graph_desc_3d> graphs;
  range_setting x_range;
  range_setting y_range;
  range_setting u_range;
  range_setting v_range;
  samples_setting samples;
  samples_setting isosamples;
  char separator;
};

struct multiplot_setting
{
  uint32_t rows;
  uint32_t cols;
};

enum class settings_id
{
  samples,
  isosamples,
  datafile_separator,
  xrange,
  parametric,
  timefmt,
  xdata,
  hidden3d,
  pallette_rgbformulae,
  multiplot
};

using all_settings =
    enum_sequence<settings_id, settings_id::samples, settings_id::isosamples,
                  settings_id::datafile_separator, settings_id::xrange, settings_id::parametric,
                  settings_id::timefmt, settings_id::xdata, settings_id::hidden3d,
                  settings_id::pallette_rgbformulae, settings_id::multiplot>;

template <settings_id>
struct settings_type
{
  // using type = void;
};

template <>
struct settings_type<settings_id::samples>
{
  using type = samples_setting;
};

template <>
struct settings_type<settings_id::isosamples>
{
  using type = samples_setting;
};

template <>
struct settings_type<settings_id::datafile_separator>
{
  using type = char;
};

template <>
struct settings_type<settings_id::xrange>
{
  using type = range_setting;
};

template <>
struct settings_type<settings_id::parametric>
{
  using type = bool;
};

template <>
struct settings_type<settings_id::timefmt>
{
  using type = std::string;
};

template <>
struct settings_type<settings_id::xdata>
{
  using type = data_type;
};

template <>
struct settings_type<settings_id::hidden3d>
{
  using type = bool;
};

template <>
struct settings_type<settings_id::pallette_rgbformulae>
{
  using type = std::tuple<int, int, int>;
};

template <>
struct settings_type<settings_id::multiplot>
{
  using type = multiplot_setting;
};

template <settings_id id>
using settings_type_t = typename settings_type<id>::type;

template <settings_id _id>
struct settings_value
{
  static constexpr settings_id id = _id;
  settings_type_t<id> value;
};

struct show_command final
{
  template <settings_id id_>
  struct show_setting
  {
    static constexpr auto id = id_;
  };

  enum_sum_t<settings_id, show_setting, all_settings> setting;
};

struct unset_command final
{
  template <settings_id id_>
  struct unset_setting
  {
    static constexpr auto id = id_;
  };

  enum_sum_t<settings_id, unset_setting, all_settings> setting;
};

struct set_command
{
  enum_sum_t<settings_id, settings_value, all_settings> value;
};

struct user_definition
{
  std::string name;
  std::optional<std::vector<std::string>> params;
  expr body;
};

struct cd_command
{
  std::filesystem::path path;
};

struct pwd_command
{
};

struct load_command
{
  std::filesystem::path path;
};

using command =
    std::variant<quit_command, plot_command_2d, plot_command_3d, user_definition, set_command,
                 show_command, unset_command, cd_command, pwd_command, load_command>;

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
