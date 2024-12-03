#include "parse_commands.hpp"
#include <ctre.hpp>
#include <string_view>
#include <charconv>
#include <cassert>
#include <fmt/format.h>
#include <lexy/dsl.hpp>
#include <lexy/callback.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy/action/parse.hpp>
#include <lexy_ext/report_error.hpp>
#include <utility>
#include "settings.hpp"
#include <numbers>
#include <algorithm>
#include "colors.hpp"
#include <ranges>

namespace
{
namespace dsl = lexy::dsl;
using namespace explot;

namespace r
{
using lexeme = lexy::string_lexeme<lexy::utf8_char_encoding>;

struct identifier : lexy::token_production
{
  static constexpr auto rule = []
  {
    auto head = dsl::ascii::alpha_underscore;
    auto tail = dsl::ascii::alpha_digit_underscore;
    return dsl::identifier(head, tail);
  }();
  static constexpr auto value = lexy::as_string<std::string>;
};
constexpr auto kw_id = dsl::identifier(dsl::ascii::alpha);

struct decimal : lexy::token_production
{
  static constexpr auto rule = dsl::peek(dsl::digit<> | dsl::lit_c<'-'>)
                               >> (dsl::opt(dsl::lit_c<'-'>) + dsl::digits<>
                                   + dsl::opt(dsl::period >> dsl::digits<>));
  static constexpr auto value = lexy::noop;
};

struct parsed_decimal : lexy::token_production
{
  static constexpr auto rule = dsl::capture(dsl::p<decimal>);
  static constexpr auto value = lexy::callback<float>(
      [](lexeme s)
      {
        float f = 0.0;
        auto [_, __] = std::from_chars(s.data(), s.data() + s.size(), f);
        return f;
      });
};

constexpr auto op_plus = dsl::op<binary_operator::plus>(dsl::lit_c<'+'>);
constexpr auto op_minus = dsl::op<binary_operator::minus>(dsl::lit_c<'-'>);
constexpr auto op_mult = dsl::op<binary_operator::mult>(dsl::lit_c<'*'>);
constexpr auto op_div = dsl::op<binary_operator::div>(dsl::lit_c<'/'>);
constexpr auto op_unary_plus = dsl::op<unary_operator::plus>(dsl::lit_c<'+'>);
constexpr auto op_unary_minus = dsl::op<unary_operator::minus>(dsl::lit_c<'-'>);

struct expr_;

struct var_or_call_
{
  struct params
  {
    static constexpr auto rule =
        dsl::parenthesized.opt_list(dsl::recurse<expr_>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_list<std::vector<expr>>;
  };

  static constexpr auto rule = dsl::p<identifier> >> dsl::if_(dsl::p<params>);
  static constexpr auto value = lexy::callback<var_or_call>(
      [](std::string name) { return var_or_call{std::move(name), std::nullopt}; },
      [](std::string name, std::vector<expr> params)
      { return var_or_call{std::move(name), std::move(params)}; });
};

struct data_ref_
{
  struct dollar_ref : lexy::token_production
  {
    static constexpr auto rule = dsl::lit_c<'$'> + dsl::integer<int>(dsl::digits<>);
    static constexpr auto value = lexy::as_integer<int>;
  };

  struct column_call
  {
    static constexpr auto rule =
        LEXY_KEYWORD("column", kw_id) + dsl::parenthesized(dsl::integer<int>(dsl::digits<>));
    static constexpr auto value = lexy::as_integer<int>;
  };

  static constexpr auto rule = dsl::peek(dsl::lit_c<'$'>) >> dsl::p<dollar_ref>
                               | dsl::peek_not(dsl::lit_c<'$'>) >> dsl::p<column_call>;
  static constexpr auto value = lexy::construct<data_ref>;
};

struct atom
{
  static constexpr auto rule =
      dsl::p<parsed_decimal> | dsl::parenthesized(dsl::recurse<expr_>)
      | dsl::peek_not(LEXY_KEYWORD("column", kw_id) | dsl::lit_c<'$'>) >> dsl::p<var_or_call_>
      | dsl::p<data_ref_>;
  static constexpr auto value = lexy::callback<expr>(
      [](float v) -> expr { return literal_expr{v}; }, lexy::forward<expr>,
      [](var_or_call v) -> expr { return std::move(v); }, [](data_ref d) -> expr { return d; });
};

struct expr_ : lexy::expression_production
{
  static constexpr auto atom = dsl::p<r::atom>;
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto max_recursion_depth = 19;

  struct infix_op : dsl::prefix_op
  {
    static constexpr auto op = op_unary_plus / op_unary_minus;
    using operand = dsl::atom;
  };

  struct mult_op : dsl::infix_op_left
  {
    static constexpr auto op = op_mult / op_div;
    using operand = infix_op;
  };

  struct add_op : dsl::infix_op_left
  {
    static constexpr auto op = op_plus / op_minus;
    using operand = mult_op;
  };

  using operation = add_op;

  static constexpr auto value = lexy::callback<expr>(
      [](unary_operator op, expr operand) -> expr { return unary_op{op, std::move(operand)}; },
      [](expr lhs, binary_operator op, expr rhs) -> expr
      { return binary_op{std::move(lhs), op, std::move(rhs)}; }, [](expr e) { return e; });
};

struct const_expr_;

struct const_var_or_call_
{
  struct pi
  {
    static constexpr auto rule = LEXY_KEYWORD("pi", kw_id);
    static constexpr auto value = lexy::constant(std::numbers::pi_v<float>);
  };

  struct sin
  {
    static constexpr auto rule = LEXY_KEYWORD("sin", kw_id)
                                 >> dsl::parenthesized(dsl::recurse<const_expr_>);
    static constexpr auto value = lexy::callback<float>([](float v) { return std::sin(v); });
  };

  static constexpr auto rule = dsl::p<pi> | dsl::p<sin>;
  static constexpr auto value = lexy::forward<float>;
};

struct const_atom
{
  static constexpr auto rule = dsl::p<parsed_decimal>
                               | dsl::parenthesized(dsl::recurse<const_expr_>)
                               | dsl::p<const_var_or_call_>;
  static constexpr auto value = lexy::forward<float>;
};

struct const_expr_ : lexy::expression_production
{
  static constexpr auto atom = dsl::p<r::const_atom>;
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto max_recursion_depth = 19;

  struct infix_op : dsl::prefix_op
  {
    static constexpr auto op = op_unary_plus / op_unary_minus;
    using operand = dsl::atom;
  };

  struct mult_op : dsl::infix_op_left
  {
    static constexpr auto op = op_mult / op_div;
    using operand = infix_op;
  };

  struct add_op : dsl::infix_op_left
  {
    static constexpr auto op = op_plus / op_minus;
    using operand = mult_op;
  };

  using operation = add_op;

  static constexpr auto value = lexy::callback<float>(
      [](unary_operator op, float v)
      {
        switch (op)
        {
        case unary_operator::plus:
          return v;
        case unary_operator::minus:
          return -v;
        }
      },
      [](float lhs, binary_operator op, float rhs)
      {
        switch (op)
        {
        case binary_operator::plus:
          return lhs + rhs;
        case binary_operator::minus:
          return lhs - rhs;
        case binary_operator::mult:
          return lhs * rhs;
        case binary_operator::div:
          return lhs / rhs;
        }
      },
      [](float e) { return e; });
};

struct named_color : lexy::scan_production<glm::vec4>, lexy::token_production
{
  struct unknown_color
  {
  };

  template <typename context, typename reader>
  static constexpr scan_result scan(lexy::rule_scanner<context, reader> &scanner)
  {
    auto begin_name = scanner.position();
    while (!scanner.peek(dsl::lit_c<'"'>))
    {
      if (scanner.is_at_eof())
      {
        return lexy::scan_failed;
      }
      else
      {
        scanner.parse(dsl::code_point);
      }
    }
    auto end_name = scanner.position();
    auto name = std::string_view(begin_name, end_name);
    auto color = get_named_color(name);
    if (color.has_value())
    {
      return color.value();
    }
    else
    {
      scanner.error(unknown_color{}, scanner.begin(), end_name);
      return lexy::scan_failed;
    }
  }
};

struct hex_color : lexy::token_production
{
  static constexpr auto
      rule = dsl::lit_c<'#'> >> dsl::position
                                    + dsl::times<6>(LEXY_ASCII_ONE_OF("0123456789abcdefABCDEF"))
                                    + dsl::position;
  static constexpr auto value = lexy::callback<glm::vec4>(
      [](const char *s, const char *e)
      {
        int rgb;
        auto _ = std::from_chars(s, e, rgb, 16);
        return from_rgb(rgb);
      });
};

struct color
{
  static constexpr auto rule =
      dsl::lit_c<'"'> + (dsl::p<hex_color> | dsl::else_ >> dsl::p<named_color>)+dsl::lit_c<'"'>;
  static constexpr auto value = lexy::forward<glm::vec4>;
};

struct line_type
{
  struct width
  {
    static constexpr auto rule = LEXY_KEYWORD("width", kw_id) >> dsl::p<parsed_decimal>;
    static constexpr auto value = lexy::forward<float>;
  };

  struct color_directive
  {
    static constexpr auto rule = LEXY_KEYWORD("rgb", kw_id) >> dsl::p<color>;
    static constexpr auto value = lexy::forward<glm::vec4>;
  };

  static constexpr auto rule = dsl::partial_combination(dsl::p<width>, dsl::p<color_directive>);
  static constexpr auto value = lexy::fold_inplace<line_type_spec>(
      [] { return line_type_spec{}; }, [](line_type_spec &lt, float width) { lt.width = width; },
      [](line_type_spec &lt, glm::vec4 color) { lt.color = color; });
};

struct line_type_or_ref
{
  static constexpr auto rule =
      dsl::digit<> >> dsl::p<parsed_decimal> | dsl::else_ >> dsl::p<line_type>;
  static constexpr auto value = lexy::callback<line_type_desc>(
      [](float idx) { return line_type_desc{static_cast<int>(idx)}; },
      [](const line_type_spec &lt) { return line_type_desc{lt}; });
};

struct range
{
  struct rvalue
  {
    static constexpr auto whitespace = dsl::ascii::space;
    static constexpr auto rule = dsl::lit_c<'*'> | (dsl::peek(dsl::lit_c<']'>) >> dsl::nullopt)
                                 | (dsl::else_ >> dsl::p<const_expr_>);
    static constexpr auto value = lexy::callback<range_value>(
        [] { return range_value(auto_scale{}); }, [](lexy::nullopt)
        { return range_value(std::nullopt); }, [](float f) { return range_value(f); });
  };
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule =
      dsl::square_bracketed(dsl::twice(dsl::p<rvalue>, dsl::sep(dsl::colon)));
  static constexpr auto value = lexy::construct<range_setting>;
};

struct ranges
{
  static constexpr auto rule = dsl::opt(dsl::list(dsl::peek(dsl::lit_c<'['>) >> dsl::p<range>));
  static constexpr auto value = lexy::as_list<std::vector<range_setting>>;
};

struct with_2d
{
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule = dsl::capture(LEXY_LIT("lines")) | dsl::capture(LEXY_LIT("points"))
                               | dsl::capture(LEXY_LIT("impulses"));
  static constexpr auto value = lexy::callback<mark_type_2d>(
      [](const auto &s)
      {
        std::string ss(s.begin(), s.end());
        if (ss == "lines")
        {
          return mark_type_2d::lines;
        }
        else if (ss == "points")
        {
          return mark_type_2d::points;
        }
        else
        {
          return mark_type_2d::impulses;
        }
      });
};

struct with_3d
{
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule = dsl::capture(LEXY_LIT("lines")) | dsl::capture(LEXY_LIT("points"))
                               | dsl::capture(LEXY_LIT("impulses"));
  static constexpr auto value = lexy::callback<mark_type_3d>(
      [](const auto &s)
      {
        std::string ss(s.begin(), s.end());
        if (ss == "lines")
        {
          return mark_type_3d::lines;
        }
        else // if (ss == "points")
        {
          return mark_type_3d::points;
        }
      });
};

struct usingp
{
  struct coord
  {
    static constexpr auto rule =
        dsl::parenthesized(dsl::p<expr_>) | dsl::integer<int>(dsl::digits<>);
    static constexpr auto value =
        lexy::callback<expr>(lexy::forward<expr>, [](int idx) -> expr { return data_ref{idx}; });
  };
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule =
      dsl::if_(LEXY_KEYWORD("using", kw_id) >> dsl::list(dsl::p<coord>, dsl::sep(dsl::colon)));
  static constexpr auto value = lexy::as_list<std::vector<expr>>;
};

constexpr auto str_delim = dsl::lit_c<'"'> | dsl::lit_c<'\''>;

struct string
{
  static constexpr auto rule = dsl::delimited(str_delim)(-dsl::unicode::control);
  static constexpr auto value = lexy::as_string<std::string>;
};

struct title
{
  static constexpr auto rule = dsl::p<string>;
  static constexpr auto value = lexy::forward<std::string>;
};

struct csv_data_
{
  static constexpr auto rule = dsl::p<string> + dsl::p<usingp>;
  static constexpr auto value =
      lexy::callback<csv_data>([](std::string path, std::vector<expr> exprs)
                               { return csv_data{std::move(path), std::move(exprs)}; });
};

constexpr auto graph_list_2d = lexy::fold_inplace<std::vector<graph_desc_2d>>(
    [] { return std::vector<graph_desc_2d>(); },
    [](std::vector<graph_desc_2d> &gs, graph_desc_2d g)
    {
      if (std::holds_alternative<csv_data>(g.data))
      {
        auto &d = std::get<csv_data>(g.data);
        if (d.path.empty())
        {
          auto rgs = std::views::reverse(gs);
          auto last_with_path =
              std::ranges::find_if(rgs,
                                   [](const graph_desc_2d &h)
                                   {
                                     return std::holds_alternative<csv_data>(h.data)
                                            && !std::get<csv_data>(h.data).path.empty();
                                   });
          if (last_with_path != rgs.end())
          {
            d.path = std::get<csv_data>(last_with_path->data).path;
          }
        }
      }
      gs.push_back(std::move(g));
    });

struct plot
{
  static constexpr auto whitespace = dsl::ascii::space;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<csv_data_>
                                 | dsl::peek_not(str_delim) >> dsl::p<expr_>;
    static constexpr auto value = lexy::construct<data_source_2d>;
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(LEXY_KEYWORD("with", kw_id) >> dsl::p<with_2d>,
                                   LEXY_KEYWORD("title", kw_id) >> dsl::p<title>,
                                   LEXY_KEYWORD("lt", kw_id) >> dsl::p<line_type_or_ref>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_2d>(
          [] { return graph_desc_2d{}; }, [](graph_desc_2d &g, mark_type_2d m) { g.mark = m; },
          [](graph_desc_2d &g, std::string title) { g.title = std::move(title); },
          [](graph_desc_2d &g, const line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_2d>(
        [](const char *s, data_source_2d d, const char *e, graph_desc_2d g)
        {
          g.data = d;
          if (g.title.empty())
          {
            while (s != e && std::isspace(*s))
            {
              ++s;
            }
            while (s != e && std::isspace(*(e - 1)))
            {
              --e;
            }
            g.title = std::string(s, e);
          }
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = graph_list_2d;
  };

  static constexpr auto rule =
      LEXY_KEYWORD("plot", kw_id) + dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<plot_command_2d>(
      [](const std::vector<range_setting> &rs, std::vector<graph_desc_2d> gs)
      {
        auto cmd = plot_command_2d{};
        if (rs.size() > 0)
        {
          cmd.x_range = rs[0];
        }
        if (rs.size() > 1)
        {
          cmd.y_range = rs[1];
        }
        cmd.graphs = std::move(gs);
        return cmd;
      });
};

struct parametric_plot
{
  static constexpr auto whitespace = dsl::ascii::space;

  using data_ast = std::variant<std::string, std::pair<expr, expr>>;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<csv_data_>
                                 | dsl::peek_not(str_delim)
                                       >> dsl::twice(dsl::p<expr_>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::callback<data_source_2d>(
        lexy::forward<csv_data>, [](expr x, expr y) { return parametric_data_2d{x, y}; });
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(LEXY_KEYWORD("with", kw_id) >> dsl::p<with_2d>,
                                   LEXY_KEYWORD("title", kw_id) >> dsl::p<title>,
                                   LEXY_KEYWORD("lt", kw_id) >> dsl::p<line_type_or_ref>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_2d>(
          [] { return graph_desc_2d{}; }, [](auto &g, mark_type_2d m) { g.mark = m; },
          [](auto &g, std::string title) { g.title = std::move(title); },
          [](graph_desc_2d &g, const line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_2d>(
        [](const char *s, data_source_2d d, const char *e, graph_desc_2d g)
        {
          g.data = std::move(d);
          if (g.title.empty())
          {
            while (s != e && std::isspace(*s))
            {
              ++s;
            }
            while (s != e && std::isspace(*(e - 1)))
            {
              --e;
            }
            g.title = std::string(s, e);
          }
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = graph_list_2d;
  };

  static constexpr auto rule =
      LEXY_KEYWORD("plot", kw_id) + dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<plot_command_2d>(
      [](const std::vector<range_setting> &rs, std::vector<graph_desc_2d> gs)
      {
        auto cmd = plot_command_2d{};
        if (rs.size() > 0)
        {
          cmd.t_range = rs[0];
        }
        if (rs.size() > 1)
        {
          cmd.x_range = rs[1];
        }
        if (rs.size() > 2)
        {
          cmd.y_range = rs[2];
        }
        cmd.graphs = std::move(gs);
        return cmd;
      });
};

constexpr auto graph_list_3d = lexy::fold_inplace<std::vector<graph_desc_3d>>(
    [] { return std::vector<graph_desc_3d>(); },
    [](std::vector<graph_desc_3d> &gs, graph_desc_3d g)
    {
      if (std::holds_alternative<csv_data>(g.data))
      {
        auto &d = std::get<csv_data>(g.data);
        if (d.path.empty())
        {
          auto rgs = std::views::reverse(gs);
          auto last_with_path =
              std::ranges::find_if(rgs,
                                   [](const graph_desc_3d &h)
                                   {
                                     return std::holds_alternative<csv_data>(h.data)
                                            && !std::get<csv_data>(h.data).path.empty();
                                   });
          if (last_with_path != rgs.end())
          {
            d.path = std::get<csv_data>(last_with_path->data).path;
          }
        }
      }
      gs.push_back(std::move(g));
    });

struct splot
{
  static constexpr auto whitespace = dsl::ascii::space;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<csv_data_>
                                 | dsl::peek_not(str_delim) >> dsl::p<expr_>;
    static constexpr auto value = lexy::construct<data_source_3d>;
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(LEXY_KEYWORD("with", kw_id) >> dsl::p<with_3d>,
                                   LEXY_KEYWORD("title", kw_id) >> dsl::p<title>,
                                   LEXY_KEYWORD("lt", kw_id) >> dsl::p<line_type_or_ref>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_3d>(
          [] { return graph_desc_3d{}; }, [](auto &g, mark_type_3d m) { g.mark = m; },
          [](auto &g, std::string title) { g.title = std::move(title); },
          [](graph_desc_3d &g, const line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_3d>(
        [](const char *s, data_source_3d d, const char *e, graph_desc_3d g)
        {
          if (g.title.empty())
          {
            while (s != e && std::isspace(*s))
            {
              ++s;
            }
            while (s != e && std::isspace(*(e - 1)))
            {
              --e;
            }
            g.title = std::string(s, e);
          }
          g.data = std::move(d);
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = graph_list_3d;
  };

  static constexpr auto rule =
      LEXY_KEYWORD("splot", kw_id) + dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<plot_command_3d>(
      [](const std::vector<range_setting> &rs, std::vector<graph_desc_3d> gs)
      {
        auto cmd = plot_command_3d{};
        if (rs.size() > 0)
        {
          cmd.x_range = rs[0];
        }
        if (rs.size() > 1)
        {
          cmd.y_range = rs[1];
        }
        cmd.graphs = std::move(gs);
        return cmd;
      });
};

struct parametric_splot
{
  static constexpr auto whitespace = dsl::ascii::space;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<csv_data_>
                                 | dsl::peek_not(str_delim)
                                       >> dsl::times<3>(dsl::p<expr_>, dsl::sep(dsl::lit_c<','>));
    static constexpr auto value =
        lexy::callback<data_source_3d>(lexy::forward<csv_data>, [](expr x, expr y, expr z)
                                       { return parametric_data_3d{x, y, z}; });
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(LEXY_KEYWORD("with", kw_id) >> dsl::p<with_3d>,
                                   LEXY_KEYWORD("title", kw_id) >> dsl::p<title>,
                                   LEXY_KEYWORD("lt", kw_id) >> dsl::p<line_type>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_3d>(
          [] { return graph_desc_3d{}; }, [](auto &g, mark_type_3d m) { g.mark = m; },
          [](auto &g, std::string title) { g.title = std::move(title); },
          [](graph_desc_3d &g, const line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_3d>(
        [](const char *s, data_source_3d d, const char *e, graph_desc_3d g)
        {
          if (g.title.empty())
          {
            while (s != e && std::isspace(*s))
            {
              ++s;
            }
            while (s != e && std::isspace(*(e - 1)))
            {
              --e;
            }
            g.title = std::string(s, e);
          }
          g.data = std::move(d);
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = graph_list_3d;
  };

  static constexpr auto rule =
      LEXY_KEYWORD("splot", kw_id) + dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<plot_command_3d>(
      [](const std::vector<range_setting> &rs, std::vector<graph_desc_3d> gs)
      {
        auto cmd = plot_command_3d{};
        if (rs.size() > 0)
        {
          cmd.u_range = rs[0];
        }
        if (rs.size() > 1)
        {
          cmd.v_range = rs[1];
        }
        if (rs.size() > 2)
        {
          cmd.x_range = rs[2];
        }
        if (rs.size() > 3)
        {
          cmd.y_range = rs[3];
        }
        cmd.graphs = std::move(gs);
        return cmd;
      });
};

struct quit
{
  static constexpr auto rule = LEXY_KEYWORD("quit", kw_id) + dsl::eof;
  static constexpr auto value = lexy::constant(quit_command{});
};

template <template <settings_id> typename parser>
struct settings_parser
{
  template <settings_id id>
  using vv = decltype(parser<id>::value)::return_type;
  static constexpr auto rule =
      (LEXY_KEYWORD("samples", kw_id) >> dsl::p<parser<settings_id::samples>>)
      | (LEXY_KEYWORD("xdata", kw_id) >> dsl::p<parser<settings_id::xdata>>)
      | (LEXY_KEYWORD("isosamples", kw_id) >> dsl::p<parser<settings_id::isosamples>>)
      | (LEXY_KEYWORD("datafile", kw_id)
         >> (LEXY_KEYWORD("separator", kw_id) >> dsl::p<parser<settings_id::datafile_separator>>))
      | (LEXY_KEYWORD("xrange", kw_id) >> dsl::p<parser<settings_id::xrange>>)
      | (LEXY_KEYWORD("parametric", kw_id) >> dsl::p<parser<settings_id::parametric>>)
      | (LEXY_KEYWORD("timefmt", kw_id) >> dsl::p<parser<settings_id::timefmt>>);
  static constexpr auto value = lexy::construct<enum_sum_t<settings_id, vv, all_settings>>;
};

struct show
{
  static constexpr auto whitespace = dsl::ascii::space;

  template <settings_id id>
  struct show_parser
  {
    static constexpr auto rule = dsl::eof;
    static constexpr auto value = lexy::constant(show_command::show_setting<id>{});
  };

  static constexpr auto rule = LEXY_KEYWORD("show", kw_id) + dsl::p<settings_parser<show_parser>>;
  static constexpr auto value = lexy::construct<show_command>;
};

struct unset
{
  static constexpr auto whitespace = dsl::ascii::space;

  template <settings_id id>
  struct unset_parser
  {
    static constexpr auto rule = dsl::eof;
    static constexpr auto value = lexy::constant(unset_command::unset_setting<id>{});
  };

  static constexpr auto rule = LEXY_KEYWORD("unset", kw_id) + dsl::p<settings_parser<unset_parser>>;
  static constexpr auto value = lexy::construct<unset_command>;
};

struct set
{
  static constexpr auto whitespace = dsl::ascii::space;

  template <typename T>
  struct value_parser
  {
  };

  template <>
  struct value_parser<samples_setting>
  {
    static constexpr auto rule = dsl::p<parsed_decimal>;
    static constexpr auto value = lexy::construct<samples_setting>;
  };

  template <>
  struct value_parser<data_type>
  {
    struct time
    {
      static constexpr auto rule = LEXY_KEYWORD("time", kw_id);
      static constexpr auto value = lexy::constant(data_type::time);
    };
    static constexpr auto rule = dsl::opt(dsl::p<time>);
    static constexpr auto value = lexy::callback<data_type>(
        [](lexy::nullopt) { return data_type::normal; }, [](data_type t) { return t; });
  };

  template <>
  struct value_parser<bool>
  {
    static constexpr auto rule = dsl::eof;
    static constexpr auto value = lexy::constant(true);
  };

  template <>
  struct value_parser<range_setting>
  {
    static constexpr auto rule = dsl::p<range>;
    static constexpr auto value = lexy::forward<range_setting>;
  };

  template <>
  struct value_parser<char>
  {
    static constexpr auto rule = dsl::p<string>;
    static constexpr auto value = lexy::callback<char>([](const std::string &s) { return s[0]; });
  };

  template <>
  struct value_parser<std::string>
  {
    static constexpr auto rule = dsl::p<string>;
    static constexpr auto value = lexy::forward<std::string>;
  };

  template <settings_id id>
  struct set_parser
  {
    static constexpr auto rule = dsl::p<value_parser<settings_type_t<id>>>;
    static constexpr auto value = lexy::construct<settings_value<id>>;
  };

  static constexpr auto rule = LEXY_KEYWORD("set", kw_id) + dsl::p<settings_parser<set_parser>>;
  static constexpr auto value = lexy::construct<set_command>;
};
} // namespace r

} // namespace

namespace explot
{
std::optional<command> parse_command(const char *cmd)
{
  auto line = std::string_view(cmd);
  auto input = lexy::string_input<lexy::utf8_char_encoding>(line.begin(), line.end());
  if (line.starts_with("quit"))
  {
    return quit_command();
  }
  else if (line.starts_with("show "))
  {
    auto result = lexy::parse<r::show>(input, lexy_ext::report_error);
    if (result.is_success())
    {
      return result.value();
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (line.starts_with("unset "))
  {
    auto result = lexy::parse<r::unset>(input, lexy_ext::report_error);
    if (result.is_success())
    {
      return result.value();
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (line.starts_with("set "))
  {
    auto result = lexy::parse<r::set>(input, lexy_ext::report_error);
    if (result.is_success())
    {
      return result.value();
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (line.starts_with("plot "))
  {
    auto matched = settings::parametric()
                       ? lexy::parse<r::parametric_plot>(input, lexy_ext::report_error)
                       : lexy::parse<r::plot>(input, lexy_ext::report_error);
    if (matched.is_success())
    {
      return matched.value();
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (line.starts_with("splot"))
  {
    auto matched = settings::parametric()
                       ? lexy::parse<r::parametric_splot>(input, lexy_ext::report_error)
                       : lexy::parse<r::splot>(input, lexy_ext::report_error);
    if (matched.is_success())
    {
      return matched.value();
    }
    else
    {
      return std::nullopt;
    }
  }
  else
  {
    return std::nullopt;
  }
}

std::optional<range_setting> parse_range_setting(std::string_view s)
{
  auto input = lexy::string_input<lexy::utf8_char_encoding>(s);
  auto matched = lexy::parse<r::range>(input, lexy_ext::report_error);
  if (matched.is_success())
  {
    return matched.value();
  }
  else
  {
    return std::nullopt;
  }
}
} // namespace explot
