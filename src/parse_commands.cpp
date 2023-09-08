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
#include "overload.hpp"
#include <numbers>

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
      [](std::string name) {
        return var_or_call{std::move(name), std::nullopt};
      },
      [](std::string name, std::vector<expr> params) {
        return var_or_call{std::move(name), std::move(params)};
      });
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
      [](unary_operator op, expr operand) -> expr {
        return unary_op{op, std::move(operand)};
      },
      [](expr lhs, binary_operator op, expr rhs) -> expr {
        return binary_op{std::move(lhs), op, std::move(rhs)};
      },
      [](expr e) { return e; });
};

// struct captured_expr
// {
//   static constexpr auto rule = dsl::position + dsl::p<expr_> + dsl::position;
//   static constexpr auto value = lexy::callback<lexeme>(
//       [](const char *s, const expr &, const char *e) { return lexeme(s, e); });
// };

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

struct range
{
  struct rvalue
  {
    static constexpr auto rule =
        dsl::opt(dsl::lit_c<'*'> | (dsl::peek_not(dsl::lit_c<'*'>) >> dsl::p<const_expr_>));
    static constexpr auto value =
        lexy::callback<range_value>([] { return range_value(auto_scale{}); },
                                    [](lexy::nullopt) { return range_value(std::nullopt); },
                                    [](float f) { return range_value(f); });
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

struct with
{
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule = LEXY_KEYWORD("with", kw_id) >> (dsl::capture(LEXY_LIT("lines"))
                                                               | dsl::capture(LEXY_LIT("points")));
  static constexpr auto value = lexy::callback<mark_type>(
      [](const auto &s)
      {
        std::string ss(s.begin(), s.end());
        if (ss == "lines")
        {
          return mark_type::lines;
        }
        else
        {
          return mark_type::points;
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

struct plot
{
  static constexpr auto whitespace = dsl::ascii::space;

  using data_ast = std::variant<expr, std::string>;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim) >> dsl::p<expr_>;
    static constexpr auto value = lexy::construct<data_ast>;
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(dsl::peek(LEXY_KEYWORD("with", kw_id)) >> dsl::p<with>,
                                   dsl::peek(LEXY_KEYWORD("using", kw_id)) >> dsl::p<usingp>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_2d>(
          [] { return graph_desc_2d{}; }, [](auto &g, mark_type m) { g.mark = m; },
          [](auto &g, const std::vector<expr> &exprs) {
            g.data = csv_data_2d{"", {exprs[0], exprs[1]}};
          });
    };

    static constexpr auto rule = dsl::p<data> + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_2d>(
        [](data_ast d, graph_desc_2d g)
        {
          std::visit(overload(
                         [&](std::string &&path)

                         {
                           if (std::holds_alternative<csv_data_2d>(g.data))
                           {
                             std::get<csv_data_2d>(g.data).path = std::move(path);
                           }
                           else
                           {
                             g.data = csv_data_2d{std::move(path), {data_ref{1}, data_ref{2}}};
                           }
                         },
                         [&](expr &&e)
                         {
                           assert(std::holds_alternative<expr>(g.data));
                           g.data = std::move(e);
                         }),
                     std::move(d));
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_list<std::vector<graph_desc_2d>>;
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
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim)
                                       >> dsl::twice(dsl::p<expr_>, dsl::sep(dsl::comma));
    static constexpr auto value =
        lexy::callback<data_ast>([](std::string s) -> data_ast { return data_ast(std::move(s)); },
                                 [](expr x, expr y) -> data_ast
                                 { return data_ast(std::make_pair(std::move(x), std::move(y))); });
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(dsl::peek(LEXY_KEYWORD("with", kw_id)) >> dsl::p<with>,
                                   dsl::peek(LEXY_KEYWORD("using", kw_id)) >> dsl::p<usingp>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_2d>(
          [] { return graph_desc_2d{}; }, [](auto &g, mark_type m) { g.mark = m; },
          [](auto &g, const std::vector<expr> &exprs)
          {
            auto &d = std::get<csv_data_2d>(g.data);
            d.expressions[0] = exprs[0];
            d.expressions[1] = exprs[1];
          });
    };

    static constexpr auto rule = dsl::p<data> + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_2d>(
        [](data_ast d, graph_desc_2d g)
        {
          if (std::holds_alternative<std::string>(d))
          {
            if (std::holds_alternative<csv_data_2d>(g.data))
            {
              std::get<csv_data_2d>(g.data).path = std::move(std::get<std::string>(d));
            }
            else
            {
              g.data = csv_data_2d{std::move(std::get<std::string>(d)), {data_ref{1}, data_ref{2}}};
            }
          }
          else
          {
            assert(std::holds_alternative<expr>(g.data));
            auto &p = std::get<std::pair<expr, expr>>(d);
            g.data = parametric_data_2d{std::move(p.first), std::move(p.second)};
          }
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_list<std::vector<graph_desc_2d>>;
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

struct splot
{
  static constexpr auto whitespace = dsl::ascii::space;

  using data_ast = std::variant<expr, std::string>;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim) >> dsl::p<expr_>;
    static constexpr auto value = lexy::forward<data_ast>;
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(dsl::peek(LEXY_KEYWORD("with", kw_id)) >> dsl::p<with>,
                                   dsl::peek(LEXY_KEYWORD("using", kw_id)) >> dsl::p<usingp>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_3d>(
          [] { return graph_desc_3d{}; }, [](auto &g, mark_type m) { g.mark = m; },
          [](auto &g, const std::vector<expr> &exprs)
          {
            auto &d = std::get<csv_data_3d>(g.data);
            d.expressions[0] = exprs[0];
            d.expressions[1] = exprs[1];
            d.expressions[2] = exprs[2];
          });
    };

    static constexpr auto rule = dsl::p<data> + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_3d>(
        [](data_ast d, graph_desc_3d g)
        {
          std::visit(overload(
                         [&](std::string path)
                         {
                           if (std::holds_alternative<csv_data_3d>(g.data))
                           {
                             std::get<csv_data_3d>(g.data).path = std::move(path);
                           }
                           else
                           {
                             g.data = csv_data_3d{std::move(path),
                                                  {data_ref{1}, data_ref{2}, data_ref{3}}};
                           }
                         },
                         [&](expr e)
                         {
                           assert(std::holds_alternative<expr>(g.data));
                           g.data = std::move(e);
                         }),
                     std::move(d));
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_list<std::vector<graph_desc_3d>>;
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

  using data_ast = std::variant<std::string, std::array<expr, 3>>;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim)
                                       >> dsl::times<3>(dsl::p<expr_>, dsl::sep(dsl::lit_c<','>));
    static constexpr auto value = lexy::callback<data_ast>(
        [](const std::string &s) { return s; },
        [](expr x, expr y, expr z) {
          return std::array<expr, 3>{std::move(x), std::move(y), std::move(z)};
        });
  };

  struct graph
  {
    static constexpr auto whitespace = dsl::ascii::space;
    struct directives
    {
      static constexpr auto whitespace = dsl::ascii::space;
      static constexpr auto rule =
          dsl::partial_combination(dsl::peek(LEXY_KEYWORD("with", kw_id)) >> dsl::p<with>,
                                   dsl::peek(LEXY_KEYWORD("using", kw_id)) >> dsl::p<usingp>);
      static constexpr auto value = lexy::fold_inplace<graph_desc_3d>(
          [] { return graph_desc_3d{}; }, [](auto &g, mark_type m) { g.mark = m; },
          [](auto &g, const std::vector<expr> &exprs)
          {
            auto &d = std::get<csv_data_3d>(g.data);
            d.expressions[0] = exprs[0];
            d.expressions[1] = exprs[1];
            d.expressions[2] = exprs[2];
          });
    };

    static constexpr auto rule = dsl::p<data> + dsl::p<directives>;
    static constexpr auto value = lexy::callback<graph_desc_3d>(
        [](data_ast d, graph_desc_3d g)
        {
          std::visit(overload(
                         [&](const std::string &path) mutable
                         {
                           if (std::holds_alternative<csv_data_3d>(g.data))
                           {
                             std::get<csv_data_3d>(g.data).path = std::move(path);
                           }
                           else
                           {
                             g.data = csv_data_3d{std::move(path),
                                                  {data_ref{1}, data_ref{2}, data_ref{3}}};
                           }
                         },
                         [&](const std::array<expr, 3> &exprs) mutable
                         {
                           assert(!std::holds_alternative<csv_data_3d>(g.data));
                           g.data = parametric_data_3d{std::move(exprs[0]), std::move(exprs[1]),
                                                       std::move(exprs[2])};
                         }),
                     d);
          return g;
        });
  };

  struct graphs
  {
    static constexpr auto rule = dsl::list(dsl::p<graph>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_list<std::vector<graph_desc_3d>>;
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
} // namespace r
std::vector<std::string> split_string(std::string_view s)
{
  auto result = std::vector<std::string>();
  while (!s.empty())
  {
    auto p = s.find(' ');
    if (p == s.npos)
    {
      p = s.size();
    }
    if (p > 0)
    {
      result.emplace_back(s.substr(0, p));
    }
    s.remove_prefix(std::min(s.size(), p + 1));
  }
  return result;
}

static constexpr char settings_path_regex[] = "(\\s+[a-zA-z]+)+";

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
    if (auto [whole, p] = ctre::starts_with<settings_path_regex>(line.substr(4)); whole)
    {
      return show_command{.path = split_string(whole)};
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (line.starts_with("set "))
  {
    if (auto [whole, p] = ctre::starts_with<settings_path_regex>(line.substr(3)); whole)
    {
      auto value = line.substr(3 + whole.size());
      while (!value.empty() && value[0] == ' ')
      {
        value.remove_prefix(1);
      }
      return set_command{.path = split_string(whole), .value = std::string(value)};
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
    fmt::print("matched: {}\n", matched.is_success());
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
    fmt::print("matched: {}\n", matched.is_success());
    if (matched.is_success())
    {
      // auto &d = std::get<parametric_data_3d>(matched.value().graphs[0].data);
      // fmt::print("splot with x={},y={},z={}\n", d.x_expression, d.y_expression,
      // d.z_expression);
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
} // namespace explot
