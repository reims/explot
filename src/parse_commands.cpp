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

namespace
{
namespace dsl = lexy::dsl;
using namespace explot;

namespace r
{
using lexeme = lexy::string_lexeme<lexy::utf8_char_encoding>;

struct identifier : lexy::token_production
{
  static constexpr auto rule = dsl::list(dsl::ascii::alpha);
  static constexpr auto value = lexy::noop;
};

struct decimal : lexy::token_production
{
  static constexpr auto rule = dsl::peek(dsl::digit<> | dsl::lit_c<'-'>)
                               >> (dsl::opt(dsl::lit_c<'-'>) + dsl::digits<>
                                   + dsl::opt(dsl::period >> dsl::digits<>));
  static constexpr auto value = lexy::noop;
};

constexpr auto op_plus = dsl::op(dsl::lit_c<'+'>);
constexpr auto op_minus = dsl::op(dsl::lit_c<'-'>);
constexpr auto op_mult = dsl::op(dsl::lit_c<'*'>);
constexpr auto op_div = dsl::op(dsl::lit_c<'/'>);

struct expr;

struct var_or_fun_call
{
  static constexpr auto
      rule = dsl::p<identifier> >> dsl::if_(dsl::parenthesized.opt_list(dsl::recurse<expr>,
                                                                        dsl::sep(dsl::comma)));
  static constexpr auto value = lexy::noop;
};

struct atom
{
  static constexpr auto rule =
      dsl::p<decimal> | dsl::parenthesized(dsl::recurse<expr>) | dsl::p<var_or_fun_call>;
  static constexpr auto value = lexy::noop;
};

struct expr : lexy::expression_production
{
  static constexpr auto atom = dsl::p<r::atom>;
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto max_recursion_depth = 19;

  struct infix_op : dsl::prefix_op
  {
    static constexpr auto op = op_plus / op_minus;
    using operand = dsl::atom;
    static constexpr auto value = lexy::noop;
  };

  struct mult_op : dsl::infix_op_left
  {
    static constexpr auto op = op_mult / op_div;
    using operand = infix_op;
    static constexpr auto value = lexy::noop;
  };

  struct add_op : dsl::infix_op_left
  {
    static constexpr auto op = op_plus / op_minus;
    using operand = mult_op;
    static constexpr auto value = lexy::noop;
  };

  using operation = add_op;

  static constexpr auto value = lexy::noop;
};

struct captured_expr
{
  static constexpr auto rule = dsl::position + dsl::p<expr> + dsl::position;
  static constexpr auto value = lexy::construct<lexeme>;
};

struct range
{
  struct rvalue
  {
    static constexpr auto rule = dsl::opt(dsl::lit_c<'*'> | dsl::capture(dsl::p<decimal>));
    static constexpr auto value =
        lexy::callback<range_value>([] { return range_value(auto_scale{}); },
                                    [](lexy::nullopt) { return range_value(std::nullopt); },
                                    [](lexeme s)
                                    {
                                      float f = 0.0;
                                      auto [_, __] =
                                          std::from_chars(s.data(), s.data() + s.size(), f);
                                      return range_value(f);
                                    });
  };
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule = []
  {
    auto value = dsl::p<rvalue>;
    return dsl::square_bracketed(dsl::twice(value, dsl::sep(dsl::colon)));
  }();
  using p = std::pair<range_setting, int>;
  static constexpr auto value = lexy::construct<range_setting>;
};

struct ranges
{
  static constexpr auto rule = dsl::opt(dsl::list(dsl::peek(dsl::lit_c<'['>) >> dsl::p<range>));
  static constexpr auto value = lexy::as_list<std::vector<range_setting>>;
};

constexpr auto kw_id = dsl::identifier(dsl::ascii::alpha);

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
        dsl::parenthesized(dsl::p<captured_expr>) | dsl::capture(dsl::digits<>);
    static constexpr auto value = lexy::callback<std::string>(
        lexy::as_string<std::string>, [](const auto &s) { return fmt::format("${}", s); });
  };
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule =
      dsl::if_(LEXY_KEYWORD("using", kw_id) >> dsl::list(dsl::p<coord>, dsl::sep(dsl::colon)));
  static constexpr auto value = lexy::as_list<std::vector<std::string>>;
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

  struct data_ast
  {
    std::string val;
    bool is_path;
  };

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim) >> dsl::p<captured_expr>;
    static constexpr auto value = lexy::callback<data_ast>(
        [](const std::string &s) {
          return data_ast{s, true};
        },
        [](lexeme s) {
          return data_ast{std::string(s.begin(), s.end()), false};
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
      static constexpr auto value = lexy::fold_inplace<graph_desc_2d>(
          [] { return graph_desc_2d{}; }, [](auto &g, mark_type m) { g.mark = m; },
          [](auto &g, const std::vector<std::string> &exprs)
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
          if (d.is_path)
          {
            if (std::holds_alternative<csv_data_2d>(g.data))
            {
              std::get<csv_data_2d>(g.data).path = std::move(d.val);
            }
            else
            {
              g.data = csv_data_2d{std::move(d.val), {"$1", "$2"}};
            }
          }
          else
          {
            assert(std::holds_alternative<std::string>(g.data));
            g.data = std::move(d.val);
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

  using data_ast = std::variant<std::string, std::pair<std::string, std::string>>;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim)
                                       >> dsl::twice(dsl::p<captured_expr>, dsl::sep(dsl::comma));
    static constexpr auto value =
        lexy::callback<data_ast>([](const std::string &s) { return data_ast(s); },
                                 [](lexeme x, lexeme y)
                                 {
                                   return data_ast(std::make_pair(std::string(x.begin(), x.end()),
                                                                  std::string(y.begin(), y.end())));
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
      static constexpr auto value = lexy::fold_inplace<graph_desc_2d>(
          [] { return graph_desc_2d{}; }, [](auto &g, mark_type m) { g.mark = m; },
          [](auto &g, const std::vector<std::string> &exprs)
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
              g.data = csv_data_2d{std::move(std::get<std::string>(d)), {"$1", "$2"}};
            }
          }
          else
          {
            assert(std::holds_alternative<std::string>(g.data));
            auto &p = std::get<std::pair<std::string, std::string>>(d);
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

  struct data_ast
  {
    std::string val;
    bool is_path;
  };

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim) >> dsl::p<captured_expr>;
    static constexpr auto value = lexy::callback<data_ast>(
        [](const std::string &s) {
          return data_ast{s, true};
        },
        [](lexeme s) {
          return data_ast{std::string(s.begin(), s.end()), false};
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
          [](auto &g, const std::vector<std::string> &exprs)
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
          if (d.is_path)
          {
            if (std::holds_alternative<csv_data_3d>(g.data))
            {
              std::get<csv_data_3d>(g.data).path = std::move(d.val);
            }
            else
            {
              g.data = csv_data_3d{std::move(d.val), {"$1", "$2", "$3"}};
            }
          }
          else
          {
            assert(std::holds_alternative<std::string>(g.data));
            g.data = std::move(d.val);
          }
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

  using data_ast = std::variant<std::string, std::array<std::string, 3>>;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<string>
                                 | dsl::peek_not(str_delim)
                                       >> dsl::times<3>(dsl::p<captured_expr>,
                                                        dsl::sep(dsl::lit_c<','>));
    static constexpr auto value = lexy::callback<data_ast>([](const std::string &s) { return s; },
                                                           [](lexeme x, lexeme y, lexeme z)
                                                           {
                                                             return std::array<std::string, 3>{
                                                                 std::string(x.begin(), x.end()),
                                                                 std::string(y.begin(), y.end()),
                                                                 std::string(z.begin(), z.end())};
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
          [](auto &g, const std::vector<std::string> &exprs)
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
                             g.data = csv_data_3d{std::move(path), {"$1", "$2", "$3"}};
                           }
                         },
                         [&](const std::array<std::string, 3> &exprs) mutable
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

struct true_
{
  static constexpr auto rule = LEXY_KEYWORD("true", kw_id);
  static constexpr auto value = lexy::constant(true);
};
struct false_
{
  static constexpr auto rule = LEXY_KEYWORD("false", kw_id);
  static constexpr auto value = lexy::constant(false);
};
struct boolean
{
  static constexpr auto rule = dsl::p<true_> | dsl::p<false_>;
  static constexpr auto value = lexy::forward<bool>;
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
      // fmt::print("splot with x={},y={},z={}\n", d.x_expression, d.y_expression, d.z_expression);
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
