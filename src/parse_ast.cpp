#include "parse_ast.hpp"
#include "commands.hpp"
// clang 19 implements P1907R1 only partially and the feature flag is missing.
// the implementation is enough for nttp use in lexy, though
#define LEXY_HAS_NTTP 1
#include <lexy/dsl.hpp>
#include <lexy/callback.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy/action/parse.hpp>
#include <lexy_ext/report_error.hpp>
#include <charconv>
#include <ranges>
#include "colors.hpp"
#include "settings.hpp"
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
        std::from_chars(s.data(), s.data() + s.size(), f);
        return f;
      });
};

struct decimal_integer
{
  static constexpr auto rule = dsl::integer<uint32_t>(dsl::digits<>);
  static constexpr auto value = lexy::as_integer<uint32_t>;
};

struct signed_integer
{
  static constexpr auto rule = dsl::minus_sign + dsl::integer<int>;
  static constexpr auto value = lexy::as_integer<int>;
};

constexpr auto op_plus = dsl::op<ast::binary_operator::plus>(dsl::lit_c<'+'>);
constexpr auto op_minus = dsl::op<ast::binary_operator::minus>(dsl::lit_c<'-'>);
constexpr auto op_mult = dsl::op<ast::binary_operator::mult>(dsl::lit_c<'*'>);
constexpr auto op_div = dsl::op<ast::binary_operator::div>(dsl::lit_c<'/'>);
constexpr auto op_pow = dsl::op<ast::binary_operator::power>(LEXY_LIT("**"));
constexpr auto op_unary_plus = dsl::op<ast::unary_operator::plus>(dsl::lit_c<'+'>);
constexpr auto op_unary_minus = dsl::op<ast::unary_operator::minus>(dsl::lit_c<'-'>);

struct expr_;

struct var_or_call_
{
  struct params
  {
    static constexpr auto rule =
        dsl::parenthesized.opt_list(dsl::recurse<expr_>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_list<std::vector<ast::expr>>;
  };

  static constexpr auto rule = dsl::p<identifier> >> dsl::if_(dsl::p<params>);
  static constexpr auto value = lexy::callback<ast::var_or_call>(
      [](std::string name) { return ast::var_or_call{std::move(name), std::nullopt}; },
      [](std::string name, std::vector<ast::expr> params)
      { return ast::var_or_call{std::move(name), std::move(params)}; });
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
  static constexpr auto value = lexy::construct<ast::data_ref>;
};

struct atom
{
  static constexpr auto rule =
      dsl::p<parsed_decimal> | dsl::parenthesized(dsl::recurse<expr_>)
      | dsl::peek_not(LEXY_KEYWORD("column", kw_id) | dsl::lit_c<'$'>) >> dsl::p<var_or_call_>
      | dsl::p<data_ref_>;
  static constexpr auto value = lexy::callback<ast::expr>(
      [](float v) -> ast::expr { return ast::literal_expr{v}; }, lexy::forward<ast::expr>,
      [](ast::var_or_call v) -> ast::expr { return std::move(v); },
      [](ast::data_ref d) -> ast::expr { return d; });
};

struct expr_ : lexy::expression_production
{
  static constexpr auto atom = dsl::p<r::atom>;
  static constexpr auto whitespace = dsl::ascii::space;

  struct power_op : dsl::infix_op_left
  {
    static constexpr auto op = op_pow;
    using operand = dsl::atom;
  };

  struct infix_op : dsl::prefix_op
  {
    static constexpr auto op = op_unary_plus / op_unary_minus;
    using operand = power_op;
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

  static constexpr auto value = lexy::callback<ast::expr>(
      [](ast::unary_operator op, ast::expr operand) -> ast::expr
      { return ast::unary_op{op, std::move(operand)}; },
      [](ast::expr lhs, ast::binary_operator op, ast::expr rhs) -> ast::expr
      { return ast::binary_op{std::move(lhs), op, std::move(rhs)}; },
      [](ast::expr e) { return e; });
};

template <size_t N>
struct nttp_str
{
  char data[N];

  constexpr nttp_str(const char *d)
  {
    for (auto i = 0u; i < N; ++i)
    {
      data[i] = d[i];
    }
  }

  constexpr std::string_view as_sv() const { return {data, N - 1}; }
};

template <size_t N>
nttp_str(const char (&d)[N]) -> nttp_str<N>;

template <nttp_str name_, float value_>
struct constant_builtin
{
  static constexpr auto name = name_;
  static constexpr auto value = value_;
};

static constexpr auto constant_builtins =
    std::make_tuple(constant_builtin<"pi", std::numbers::pi_v<float>>{},
                    constant_builtin<"e", std::numbers::e_v<float>>{});

template <size_t I>
using constant_builtin_t = std::remove_cvref_t<decltype(std::get<I>(constant_builtins))>;

std::optional<float> find_constant_(std::string_view, std::index_sequence<>)
{
  return std::nullopt;
}
template <size_t I, size_t... Is>
std::optional<float> find_constant_(std::string_view name, std::index_sequence<I, Is...>)
{
  if (name == constant_builtin_t<I>::name.as_sv())
  {
    return constant_builtin_t<I>::value;
  }
  else
  {
    return find_constant_(name, std::index_sequence<Is...>{});
  }
}

template <nttp_str name_, float (*func_)(float)>
struct unary_builtin
{
  static constexpr auto name = name_;
  static constexpr auto func = func_;
};

static constexpr auto unary_builtins =
    std::make_tuple(unary_builtin<"abs", std::abs>{}, unary_builtin<"acos", std::acos>{},
                    unary_builtin<"acosh", std::acosh>{}, unary_builtin<"asin", std::asin>{},
                    unary_builtin<"asinh", std::asinh>{}, unary_builtin<"atan", std::atan>{},
                    unary_builtin<"atanh", std::atanh>{}, unary_builtin<"ceil", std::ceil>{},
                    unary_builtin<"cos", std::cos>{}, unary_builtin<"cosh", std::cosh>{},
                    unary_builtin<"exp", std::exp>{}, unary_builtin<"floor", std::floor>{},
                    unary_builtin<"log", std::log>{}, unary_builtin<"log10", std::log10>{},
                    unary_builtin<"sgn", [](float v) { return std::copysign(1.0f, v); }>{},
                    unary_builtin<"sin", std::sin>{}, unary_builtin<"sinh", std::sinh>{},
                    unary_builtin<"sqrt", std::sqrt>{}, unary_builtin<"tan", std::tan>{},
                    unary_builtin<"tanh", std::tanh>{});

template <size_t i>
using unary_builtin_t = std::remove_cvref_t<decltype(std::get<i>(unary_builtins))>;

std::optional<std::string_view> find_unary_(std::string_view, std::index_sequence<>)
{
  return std::nullopt;
}

template <size_t I, size_t... Is>
std::optional<std::string_view> find_unary_(std::string_view name, std::index_sequence<I, Is...>)
{
  if (name == unary_builtin_t<I>::name.as_sv())
  {
    return name;
  }
  else
  {
    return find_unary_(name, std::index_sequence<Is...>{});
  }
}

template <nttp_str name_, float (*func_)(float, float)>
struct binary_builtin
{
  static constexpr auto name = name_;
  static constexpr auto func = func_;
};

static constexpr auto binary_builtins =
    std::make_tuple(binary_builtin<"atan2", std::atan2>{}, binary_builtin<"pow", std::pow>{});

template <size_t I>
using binary_builtin_t = std::remove_cvref_t<decltype(std::get<I>(binary_builtins))>;

std::optional<std::string_view> find_binary_builtin(std::string_view, std::index_sequence<>)
{
  return std::nullopt;
}

template <size_t I, size_t... Is>
std::optional<std::string_view> find_binary_builtin(std::string_view name,
                                                    std::index_sequence<I, Is...>)
{
  if (name == binary_builtin_t<I>::name.as_sv())
  {
    return binary_builtin_t<I>::name.as_sv();
  }
  else
  {
    return find_binary_builtin(name, std::index_sequence<Is...>{});
  }
}

template <template <size_t> typename parser, size_t... Is>
struct disjunction_
{
  static constexpr auto rule = (dsl::p<parser<Is>> | ...);
  static constexpr auto value = lexy::forward<float>;
};

template <template <size_t> typename parser, size_t... Is>
disjunction_<parser, Is...> make_disjunction(std::index_sequence<Is...>);

template <template <size_t> typename parser, size_t N>
using disjunction = decltype(make_disjunction<parser>(std::make_index_sequence<N>()));

struct const_expr_;

struct const_var_or_call_
{
  template <size_t I>
  struct const_parser
  {
    static constexpr auto rule = dsl::keyword<constant_builtin_t<I>::name.data>(kw_id);
    static constexpr auto value = lexy::constant(constant_builtin_t<I>::value);
  };

  using constant = disjunction<const_parser, std::tuple_size_v<decltype(constant_builtins)>>;

  template <size_t I>
  struct unary_parser
  {
    static constexpr auto rule = dsl::keyword<unary_builtin_t<I>::name.data>(kw_id)
                                 >> dsl::parenthesized(dsl::recurse<const_expr_>);

    static constexpr auto value =
        lexy::callback<float>([](float v) { return unary_builtin_t<I>::func(v); });
  };

  using unaries = disjunction<unary_parser, std::tuple_size_v<decltype(unary_builtins)>>;

  template <size_t I>
  struct binary_parser
  {
    static constexpr auto rule = dsl::keyword<binary_builtin_t<I>::name.data>(kw_id)
                                 >> dsl::parenthesized(dsl::times<2>(dsl::recurse<const_expr_>,
                                                                     dsl::sep(dsl::comma)));

    static constexpr auto value =
        lexy::callback<float>([](float v1, float v2) { return binary_builtin_t<I>::func(v1, v2); });
  };

  using binaries = disjunction<binary_parser, std::tuple_size_v<decltype(binary_builtins)>>;

  static constexpr auto rule = dsl::p<constant> | dsl::p<unaries> | dsl::p<binaries>;
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

  struct power_op : dsl::infix_op_left
  {
    static constexpr auto op = op_pow;
    using operand = dsl::atom;
  };

  struct infix_op : dsl::prefix_op
  {
    static constexpr auto op = op_unary_plus / op_unary_minus;
    using operand = power_op;
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
      [](ast::unary_operator op, float v)
      {
        switch (op)
        {
        case ast::unary_operator::plus:
          return v;
        case ast::unary_operator::minus:
          return -v;
        }
      },
      [](float lhs, ast::binary_operator op, float rhs)
      {
        switch (op)
        {
        case ast::binary_operator::plus:
          return lhs + rhs;
        case ast::binary_operator::minus:
          return lhs - rhs;
        case ast::binary_operator::mult:
          return lhs * rhs;
        case ast::binary_operator::div:
          return lhs / rhs;
        case ast::binary_operator::power:
          return std::pow(lhs, rhs);
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

  struct dash_type_string
  {

    static constexpr auto rule =
        dsl::quoted(dsl::lit_c<'.'> / dsl::lit_c<'-'> / dsl::lit_c<'_'> / dsl::lit_c<' '>);

    static constexpr auto value = lexy::fold_inplace<dash_type>([]()
        { return dash_type{};
        },
                                      [](dash_type &result, const lexeme &l)
                                      {
                                        for (auto c : l)
                                        {
                                          switch (c)
                                          {
                                          case '.':
                                            result.segments.emplace_back(2, 5);
                                            break;
                                          case '-':
                                            result.segments.emplace_back(10, 10);
                                            break;
                                          case '_':
                                            result.segments.emplace_back(20, 10);
                                            break;
                                          case ' ':
                                            if (!result.segments.empty())
                                            {
                                              result.segments.back().second += 10;
                                            }
                                            break;
                                          }
                                        }
      });
  };

  struct dash_type_numerical
  {
    struct pair
    {
      static constexpr auto rule = dsl::twice(dsl::p<decimal_integer>, dsl::sep(dsl::lit_c<','>));
      static constexpr auto value = lexy::construct<std::pair<uint32_t, uint32_t>>;
    };
    static constexpr auto rule =
        dsl::parenthesized(dsl::list(dsl::p<pair>, dsl::sep(dsl::lit_c<','>)));
    static constexpr auto value =
        lexy::as_list<std::vector<std::pair<uint32_t, uint32_t>>> >> lexy::construct<dash_type>;
  };

  struct dash_type_solid
  {
    static constexpr auto rule = dsl::keyword<"solid">(kw_id);
    static constexpr auto value = lexy::constant(solid{});
  };

  struct dash_type_directive
  {
    static constexpr auto rule = LEXY_KEYWORD("dashtype", kw_id)
                                 >> (dsl::p<dash_type_string> | dsl::p<dash_type_numerical>
                                     | dsl::p<dash_type_solid> | dsl::p<decimal_integer>);
    static constexpr auto value = lexy::construct<dash_type_desc>;
  };

  static constexpr auto rule =
      dsl::partial_combination(dsl::p<width>, dsl::p<color_directive>, dsl::p<dash_type_directive>);
  static constexpr auto value = lexy::fold_inplace<ast::line_type_spec>(
      [] { return ast::line_type_spec{}; }, [](ast::line_type_spec &lt, float width)
      { lt.width = width; }, [](ast::line_type_spec &lt, glm::vec4 color) { lt.color = color; },
      [](ast::line_type_spec &lt, dash_type_desc dt) { lt.dash_type = dt; });
};

struct line_type_or_ref
{
  static constexpr auto rule =
      dsl::digit<> >> dsl::p<parsed_decimal> | dsl::else_ >> dsl::p<line_type>;
  static constexpr auto value = lexy::callback<ast::line_type_desc>(
      [](float idx) { return ast::line_type_desc{static_cast<uint32_t>(idx)}; },
      [](const ast::line_type_spec &lt) { return ast::line_type_desc{lt}; });
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
  static constexpr auto value = lexy::callback<ast::mark_type_2d>(
      [](const auto &s)
      {
        std::string ss(s.begin(), s.end());
        if (ss == "lines")
        {
          return ast::mark_type_2d::lines;
        }
        else if (ss == "points")
        {
          return ast::mark_type_2d::points;
        }
        else
        {
          return ast::mark_type_2d::impulses;
        }
      });
};

struct with_3d
{
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule = dsl::capture(LEXY_LIT("lines")) | dsl::capture(LEXY_LIT("points"))
                               | dsl::capture(LEXY_LIT("pm3d"));
  static constexpr auto value = lexy::callback<ast::mark_type_3d>(
      [](const auto &s)
      {
        std::string ss(s.begin(), s.end());
        if (ss == "lines")
        {
          return ast::mark_type_3d::lines;
        }
        else if (ss == "points")
        {
          return ast::mark_type_3d::points;
        }
        else
        {
          return ast::mark_type_3d::pm3d;
        }
      });
};

struct usingp
{
  struct coord
  {
    static constexpr auto rule =
        dsl::parenthesized(dsl::p<expr_>) | dsl::integer<int>(dsl::digits<>);
    static constexpr auto value = lexy::callback<ast::expr>(
        lexy::forward<ast::expr>, [](int idx) -> ast::expr { return ast::data_ref{idx}; });
  };
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule =
      dsl::if_(LEXY_KEYWORD("using", kw_id) >> dsl::list(dsl::p<coord>, dsl::sep(dsl::colon)));
  static constexpr auto value = lexy::as_list<std::vector<ast::expr>>;
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
  static constexpr auto rule =
      dsl::p<string> + dsl::opt(LEXY_KEYWORD("matrix", kw_id)) + dsl::p<usingp>;
  static constexpr auto value = lexy::callback<ast::csv_data>(
      [](std::string path, lexy::nullopt, std::vector<ast::expr> exprs)
      { return ast::csv_data{std::move(path), std::move(exprs), false}; },
      [](std::string path, std::vector<ast::expr> exprs)
      { return ast::csv_data{std::move(path), std::move(exprs), true}; });
};

constexpr auto graph_list_2d = lexy::fold_inplace<std::vector<ast::graph_desc_2d>>(
    [] { return std::vector<ast::graph_desc_2d>(); },
    [](std::vector<ast::graph_desc_2d> &gs, ast::graph_desc_2d g)
    {
      if (std::holds_alternative<ast::csv_data>(g.data))
      {
        auto &d = std::get<ast::csv_data>(g.data);
        if (d.path.empty())
        {
          auto rgs = std::views::reverse(gs);
          auto last_with_path =
              std::ranges::find_if(rgs,
                                   [](const ast::graph_desc_2d &h)
                                   {
                                     return std::holds_alternative<ast::csv_data>(h.data)
                                            && !std::get<ast::csv_data>(h.data).path.empty();
                                   });
          if (last_with_path != rgs.end())
          {
            d.path = std::get<ast::csv_data>(last_with_path->data).path;
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
    static constexpr auto value = lexy::construct<ast::data_source_2d>;
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
      static constexpr auto value = lexy::fold_inplace<ast::graph_desc_2d>(
          [] { return ast::graph_desc_2d{}; },
          [](ast::graph_desc_2d &g, ast::mark_type_2d m) { g.mark = m; },
          [](ast::graph_desc_2d &g, std::string title) { g.title = std::move(title); },
          [](ast::graph_desc_2d &g, const ast::line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<ast::graph_desc_2d>(
        [](const char *s, ast::data_source_2d d, const char *e, ast::graph_desc_2d g)
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

  static constexpr auto rule = dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<ast::plot_command_2d>(
      [](const std::vector<range_setting> &rs, std::vector<ast::graph_desc_2d> gs)
      {
        auto cmd = ast::plot_command_2d{};
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

  using data_ast = std::variant<std::string, std::pair<ast::expr, ast::expr>>;

  struct data
  {
    static constexpr auto rule = dsl::peek(str_delim) >> dsl::p<csv_data_>
                                 | dsl::peek_not(str_delim)
                                       >> dsl::twice(dsl::p<expr_>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::callback<ast::data_source_2d>(
        lexy::forward<ast::csv_data>,
        [](ast::expr x, ast::expr y) { return ast::parametric_data_2d{x, y}; });
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
      static constexpr auto value = lexy::fold_inplace<ast::graph_desc_2d>(
          [] { return ast::graph_desc_2d{}; }, [](auto &g, ast::mark_type_2d m) { g.mark = m; },
          [](auto &g, std::string title) { g.title = std::move(title); },
          [](ast::graph_desc_2d &g, const ast::line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<ast::graph_desc_2d>(
        [](const char *s, ast::data_source_2d d, const char *e, ast::graph_desc_2d g)
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

  static constexpr auto rule = dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<ast::plot_command_2d>(
      [](const std::vector<range_setting> &rs, std::vector<ast::graph_desc_2d> gs)
      {
        auto cmd = ast::plot_command_2d{};
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

constexpr auto graph_list_3d = lexy::fold_inplace<std::vector<ast::graph_desc_3d>>(
    [] { return std::vector<ast::graph_desc_3d>(); },
    [](std::vector<ast::graph_desc_3d> &gs, ast::graph_desc_3d g)
    {
      if (std::holds_alternative<ast::csv_data>(g.data))
      {
        auto &d = std::get<ast::csv_data>(g.data);
        if (d.path.empty())
        {
          auto rgs = std::views::reverse(gs);
          auto last_with_path =
              std::ranges::find_if(rgs,
                                   [](const ast::graph_desc_3d &h)
                                   {
                                     return std::holds_alternative<ast::csv_data>(h.data)
                                            && !std::get<ast::csv_data>(h.data).path.empty();
                                   });
          if (last_with_path != rgs.end())
          {
            d.path = std::get<ast::csv_data>(last_with_path->data).path;
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
    static constexpr auto value = lexy::construct<ast::data_source_3d>;
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
      static constexpr auto value = lexy::fold_inplace<ast::graph_desc_3d>(
          [] { return ast::graph_desc_3d{}; }, [](auto &g, ast::mark_type_3d m) { g.mark = m; },
          [](auto &g, std::string title) { g.title = std::move(title); },
          [](ast::graph_desc_3d &g, const ast::line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<ast::graph_desc_3d>(
        [](const char *s, ast::data_source_3d d, const char *e, ast::graph_desc_3d g)
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

  static constexpr auto rule = dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<ast::plot_command_3d>(
      [](const std::vector<range_setting> &rs, std::vector<ast::graph_desc_3d> gs)
      {
        auto cmd = ast::plot_command_3d{};
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
    static constexpr auto value = lexy::callback<ast::data_source_3d>(
        lexy::forward<ast::csv_data>,
        [](ast::expr x, ast::expr y, ast::expr z) { return ast::parametric_data_3d{x, y, z}; });
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
      static constexpr auto value = lexy::fold_inplace<ast::graph_desc_3d>(
          [] { return ast::graph_desc_3d{}; }, [](auto &g, ast::mark_type_3d m) { g.mark = m; },
          [](auto &g, std::string title) { g.title = std::move(title); },
          [](ast::graph_desc_3d &g, const ast::line_type_desc &lt) { g.line_type = lt; });
    };

    static constexpr auto rule = dsl::position + dsl::p<data> + dsl::position + dsl::p<directives>;
    static constexpr auto value = lexy::callback<ast::graph_desc_3d>(
        [](const char *s, ast::data_source_3d d, const char *e, ast::graph_desc_3d g)
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

  static constexpr auto rule = dsl::p<ranges> + dsl::p<graphs> + dsl::eof;
  static constexpr auto value = lexy::callback<ast::plot_command_3d>(
      [](const std::vector<range_setting> &rs, std::vector<ast::graph_desc_3d> gs)
      {
        auto cmd = ast::plot_command_3d{};
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
  static constexpr auto rule = dsl::eof;
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
      | (LEXY_KEYWORD("timefmt", kw_id) >> dsl::p<parser<settings_id::timefmt>>)
      | (LEXY_KEYWORD("hidden3d", kw_id) >> dsl::p<parser<settings_id::hidden3d>>)
      | (LEXY_KEYWORD("palette", kw_id) >> (LEXY_KEYWORD("rgbformulae", kw_id)
                                            >> dsl::p<parser<settings_id::pallette_rgbformulae>>))
      | (LEXY_KEYWORD("multiplot", kw_id) >> dsl::p<parser<settings_id::multiplot>>);
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

  static constexpr auto rule = dsl::p<settings_parser<show_parser>>;
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

  static constexpr auto rule = dsl::p<settings_parser<unset_parser>>;
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
    static constexpr auto rule = dsl::p<parsed_decimal> + dsl::opt(dsl::p<parsed_decimal>);
    static constexpr auto value = lexy::callback<samples_setting>(
        [](float v, lexy::nullopt)
        { return samples_setting(static_cast<uint32_t>(v), static_cast<uint32_t>(v)); },
        [](float v1, float v2)
        { return samples_setting(static_cast<uint32_t>(v1), static_cast<uint32_t>(v2)); });
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

  template <>
  struct value_parser<std::tuple<int, int, int>>
  {
    static constexpr auto rule = dsl::times<3>(dsl::p<signed_integer>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::construct<std::tuple<int, int, int>>;
  };

  template <>
  struct value_parser<multiplot_setting>
  {
    static constexpr auto rule = LEXY_KEYWORD("layout", kw_id)
                                 >> dsl::twice(dsl::p<decimal_integer>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::construct<multiplot_setting>;
  };

  template <settings_id id>
  struct set_parser
  {
    static constexpr auto rule = dsl::p<value_parser<settings_type_t<id>>>;
    static constexpr auto value = lexy::construct<settings_value<id>>;
  };

  static constexpr auto rule = dsl::p<settings_parser<set_parser>>;
  static constexpr auto value = lexy::construct<set_command>;
};

struct user_definition
{
  struct params
  {
    static constexpr auto rule =
        dsl::parenthesized(dsl::list(dsl::p<identifier>, dsl::sep(dsl::comma)));
    static constexpr auto value = lexy::as_list<std::vector<std::string>>;
  };

  static constexpr auto rule =
      dsl::p<identifier> + dsl::opt(dsl::p<params>) + dsl::lit_c<'='> + dsl::p<expr_>;
  static constexpr auto value = lexy::construct<ast::user_definition>;
};

struct command_ast
{
  static constexpr auto whitespace = dsl::ascii::space;
  static constexpr auto rule = dsl::keyword<"plot">(kw_id) >> dsl::p<plot>
                               | dsl::keyword<"splot">(kw_id) >> dsl::p<splot>
                               | dsl::keyword<"set">(kw_id) >> dsl::p<set>
                               | dsl::keyword<"unset">(kw_id) >> dsl::p<unset>
                               | dsl::keyword<"show">(kw_id) >> dsl::p<show>
                               | dsl::keyword<"quit">(kw_id) >> dsl::p<quit>
                               | dsl::else_ >> dsl::p<user_definition>;
  static constexpr auto value = lexy::construct<ast::command>;
};

struct parametric_command_ast
{
  static constexpr auto rule = dsl::keyword<"plot">(kw_id) >> dsl::p<parametric_plot>
                               | dsl::keyword<"splot">(kw_id) >> dsl::p<parametric_splot>
                               | dsl::keyword<"set">(kw_id) >> dsl::p<set>
                               | dsl::keyword<"unset">(kw_id) >> dsl::p<unset>
                               | dsl::keyword<"show">(kw_id) >> dsl::p<show>
                               | dsl::keyword<"quit">(kw_id) >> dsl::p<quit>
                               | dsl::else_ >> dsl::p<user_definition>;
  static constexpr auto value = lexy::construct<ast::command>;
};

} // namespace r
} // namespace

namespace explot
{
std::optional<ast::command> parse_command_ast(std::string_view line)
{
  auto input = lexy::string_input<lexy::utf8_char_encoding>(line.data(), line.data() + line.size());
  if (settings::parametric())
  {
    auto result = lexy::parse<r::parametric_command_ast>(input, lexy_ext::report_error);
    if (result.is_success())
    {
      return result.value();
    }
    else
    {
      return std::nullopt;
    }
  }
  else
  {
    auto result = lexy::parse<r::command_ast>(input, lexy_ext::report_error);
    if (result.is_success())
    {
      return result.value();
    }
    else
    {
      return std::nullopt;
    }
  }
}

std::optional<std::string_view> find_unary_builtin(std::string_view name)
{
  return r::find_unary_(name,
                        std::make_index_sequence<std::tuple_size_v<decltype(r::unary_builtins)>>{});
}

std::optional<float> find_constant_builtin(std::string_view name)
{
  return r::find_constant_(
      name, std::make_index_sequence<std::tuple_size_v<decltype(r::constant_builtins)>>{});
}

std::optional<std::string_view> find_binary_builtin(std::string_view name)
{
  return r::find_binary_builtin(
      name, std::make_index_sequence<std::tuple_size_v<decltype(r::binary_builtins)>>{});
}
} // namespace explot
