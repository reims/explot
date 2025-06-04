#include "parse_commands.hpp"
#include <string_view>
#include <cassert>
#include <fmt/format.h>
#include <utility>
#include <algorithm>
#include <ranges>
#include <tuple>
#include "parse_ast.hpp"
#include "overload.hpp"
#include "user_definitions.hpp"

namespace
{
using namespace explot;
auto validate_all(std::ranges::range auto r)
{
  using exp = std::ranges::range_value_t<decltype(r)>;
  using value = exp::value_type;
  using error = exp::error_type;
  auto result = std::expected<std::vector<value>, error>();
  for (auto &&e : r)
  {
    if (e.has_value())
    {
      result.value().push_back(std::move(e.value()));
    }
    else
    {
      result = std::unexpected(std::move(e.error()));
      break;
    }
  }
  return result;
}

auto moving_range(auto &c)
{
  return std::ranges::subrange(std::make_move_iterator(c.begin()),
                               std::make_move_iterator(c.end()));
}

unary_operator transform_unary_op(ast::unary_operator op)
{
  switch (op)
  {
  case ast::unary_operator::minus:
    return unary_operator::minus;
  case ast::unary_operator::plus:
    return unary_operator::plus;
  }
}

std::expected<expr, std::string>
validate_expression(ast::expr &&e, std::span<const std::string> vars, bool dataref_allowed)
{
  struct validator
  {
    std::span<const std::string> vars;
    bool dataref_allowed;

    std::expected<expr, std::string> operator()(ast::literal_expr &&l) const
    {
      return literal_expr{l.value};
    }
    std::expected<expr, std::string> operator()(box<ast::unary_op> &&o) const
    {
      return std::visit(*this, std::move(o->operand))
          .transform([&](expr &&operand)
                     { return box{unary_op{transform_unary_op(o->op), std::move(operand)}}; });
    }
    std::expected<expr, std::string> operator()(box<ast::binary_op> &&o) const
    {
      return std::visit(*this, std::move(o->lhs))
          .and_then(
              [&](expr &&lhs)
              {
                return std::visit(*this, std::move(o->rhs))
                    .transform(
                        [&](expr &&rhs) -> expr
                        {
                          switch (o->op)
                          {
                          case ast::binary_operator::div:
                            return box(
                                binary_op{std::move(lhs), binary_operator::div, std::move(rhs)});
                          case ast::binary_operator::mult:
                            return box(
                                binary_op{std::move(lhs), binary_operator::mult, std::move(rhs)});
                          case ast::binary_operator::minus:
                            return box(
                                binary_op{std::move(lhs), binary_operator::minus, std::move(rhs)});
                          case ast::binary_operator::plus:
                            return box(
                                binary_op{std::move(lhs), binary_operator::plus, std::move(rhs)});
                          case ast::binary_operator::power:
                            return box(binary_builtin_call{"pow", std::move(lhs), std::move(rhs)});
                          }
                        });
              });
    }
    std::expected<expr, std::string> operator()(box<ast::var_or_call> &&v) const
    {
      if (v->params)
      {
        if (auto uname = find_unary_builtin(v->name); uname)
        {
          if (v->params->size() != 1)
          {
            return std::unexpected(fmt::format("function {} takes 1 argument but got {} arguments.",
                                               v->name, v->params->size()));
          }
          else
          {
            return std::visit(*this, std::move(v->params->at(0)))
                .transform(
                    [&](expr &&arg)
                    {
                      return box{unary_builtin_call{std::string(uname.value()), std::move(arg)}};
                    });
          }
        }
        else if (auto bname = find_binary_builtin(v->name); bname)
        {
          if (v->params->size() != 2)
          {
            return std::unexpected(fmt::format("function {} takes 2 arguments but got {}.", v->name,
                                               v->params->size()));
          }
          else
          {
            return std::visit(*this, std::move(v->params->at(0)))
                .and_then(
                    [&](expr &&arg1)
                    {
                      return std::visit(*this, std::move(v->params->at(1)))
                          .transform(
                              [&](expr &&arg2)
                              {
                                return box(binary_builtin_call{std::string(bname.value()),
                                                               std::move(arg1), std::move(arg2)});
                              });
                    });
          }
        }
        else if (auto idx = find_user_function(v->name); idx)
        {
          auto &def = get_definition(*idx);
          if (def.params->size() != v->params->size())
          {
            return std::unexpected(fmt::format("function {} takes {} arguments but got {}.",
                                               v->name, def.params->size(), v->params->size()));
          }
          else
          {
            return validate_all(
                       moving_range(*v->params)
                       | std::views::transform(
                           [&](ast::expr &&arg)
                           { return validate_expression(std::move(arg), vars, dataref_allowed); }))
                .transform([&](std::vector<expr> &&args)
                           { return box{user_function_call{*idx, std::move(args)}}; });
          }
        }
        else
        {
          return std::unexpected(fmt::format("uknown function '{}'", v->name));
        }
      }
      else
      {
        if (auto value = find_constant_builtin(v->name); value)
        {
          return literal_expr{*value};
        }
        else if (std::ranges::find(vars, v->name) != vars.end())
        {
          return var{std::move(v->name)};
        }
        else if (auto idx = find_user_variable(v->name); idx)
        {
          return user_var_ref{*idx};
        }
        else
        {
          return std::unexpected(fmt::format("unknown variable '{}'", v->name));
        }
      }
    }
    std::expected<expr, std::string> operator()(ast::data_ref &&d) const
    {
      if (dataref_allowed)
      {
        return data_ref{d.idx};
      }
      else
      {
        return std::unexpected("columns outside of datafiles is not allowed.");
      }
    };
  };
  return std::visit(validator{vars, dataref_allowed}, std::move(e));
}

std::expected<csv_data, std::string> validate(mark_type_3d, ast::csv_data &&data)
{
  return [&] -> std::expected<std::vector<expr>, std::string>
  {
    switch (data.expressions.size())
    {
    case 0:
      if (data.matrix)
      {
        return std::vector<expr>{data_ref(1), data_ref(2), data_ref(3)};
      }
      else
      {
        return std::vector<expr>{data_ref(0), literal_expr(0.0f), data_ref(1)};
      }
    case 1:
      if (data.matrix)
      {
        return validate_expression(std::move(data.expressions[0]), {}, true)
            .transform([](expr &&e)
                       { return std::vector<expr>{data_ref(1), data_ref(2), std::move(e)}; });
      }
      else
      {
        return validate_expression(std::move(data.expressions[0]), {}, true)
            .transform(
                [](expr &&e)
                { return std::vector<expr>{data_ref(0), literal_expr(0.0f), std::move(e)}; });
      }
    case 3:
      return validate_all(
          std::move(data.expressions)
          | std::views::transform([](ast::expr &e)
                                  { return validate_expression(std::move(e), {}, true); }));
    default:
      return std::unexpected("need 1 or 3 expressions for splot");
    }
  }()
                    .transform(
                        [&](std::vector<expr> &&es)
                        {
                          return csv_data{.path = std::move(data.path),
                                          .expressions = std::move(es),
                                          .matrix = data.matrix};
                        });
}

std::expected<csv_data, std::string> validate(mark_type_2d mark, ast::csv_data &&data)
{
  return [&] -> std::expected<std::vector<expr>, std::string>
  {
    if (mark == mark_type_2d::impulses)
    {
      switch (data.expressions.size())
      {
      case 0:
        return std::vector<expr>{data_ref(0), literal_expr(0.0f), data_ref(0), data_ref(1)};
        break;
      case 1:
        return validate_expression(std::move(data.expressions[0]), {}, true)
            .transform(
                [](expr &&e)
                {
                  return std::vector<expr>{data_ref(0), literal_expr(0.0f), data_ref(0),
                                           std::move(e)};
                });
      case 2:
        return validate_expression(std::move(data.expressions[0]), {}, true)
            .and_then(
                [&](expr &&e1)
                {
                  return validate_expression(std::move(data.expressions[1]), {}, true)
                      .transform(
                          [&](expr &&e2)
                          { return std::vector<expr>{e1, literal_expr(0.0f), e1, std::move(e2)}; });
                });
      case 4:
        return validate_all(
            std::move(data.expressions)
            | std::views::transform([](ast::expr &e)
                                    { return validate_expression(std::move(e), {}, true); }));
      default:
        return std::unexpected("need 1, 2 or 4 expressions for plot with impulses");
      }
    }
    else
    {
      switch (data.expressions.size())
      {
      case 0:
        if (data.matrix)
        {
          return std::vector<expr>{data_ref(1), data_ref(2)};
        }
        else
        {
          return std::vector<expr>{data_ref(0), data_ref(1)};
        }
        break;
      case 1:
        if (data.matrix)
        {
          return validate_expression(std::move(data.expressions[0]), {}, true)
              .transform([](expr &&e) { return std::vector<expr>{data_ref(1), std::move(e)}; });
        }
        else
        {
          return validate_expression(std::move(data.expressions[0]), {}, true)
              .transform([](expr &&e) { return std::vector<expr>{data_ref(0), std::move(e)}; });
        }
      case 2:
        return validate_all(
            std::move(data.expressions)
            | std::views::transform([](ast::expr &e)
                                    { return validate_expression(std::move(e), {}, true); }));
      default:
        return std::unexpected("need 1 or 2 expressions for plot with lines or points");
      }
    }
  }()
                    .transform(
                        [&](std::vector<expr> &&es)
                        {
                          return csv_data{.path = std::move(data.path),
                                          .expressions = std::move(es),
                                          .matrix = data.matrix};
                        });
}

std::expected<expr, std::string> validate(mark_type_2d mark, ast::expr &&e)
{
  auto x = std::string("x");
  return validate_expression(std::move(e), {&x, 1}, false);
}

std::expected<expr, std::string> validate(mark_type_3d, ast::expr &&e)
{
  std::string vars[]{"x", "y"};
  return validate_expression(std::move(e), vars, false);
}

std::expected<parametric_data_2d, std::string> validate(mark_type_2d m,
                                                        ast::parametric_data_2d &&data)
{
  // TODO: implement impulses
  if (m == mark_type_2d::impulses)
  {
    return std::unexpected("impulses for parametric plots is not implemented yet.");
  }
  else
  {
    auto t = std::string("t");
    return validate_expression(std::move(data.x_expression), {&t, 1}, false)
        .and_then(
            [&](expr &&x)
            {
              return validate_expression(std::move(data.y_expression), {&t, 1}, false)
                  .transform([&](expr &&y)
                             { return parametric_data_2d{std::move(x), std::move(y)}; });
            });
  }
}

std::expected<parametric_data_3d, std::string> validate(mark_type_3d,
                                                        ast::parametric_data_3d &&data)
{
  // TODO: check that expressions are valid
  std::string vars[] = {"u", "v"};

  return validate_expression(std::move(data.x_expression), vars, false)
      .and_then(
          [&](expr &&x)
          {
            return validate_expression(std::move(data.y_expression), vars, false)
                .transform([&](expr &&y) { return std::make_tuple(x, y); });
          })
      .and_then(
          [&](auto &&t)
          {
            return validate_expression(std::move(data.z_expression), vars, false)
                .transform(
                    [&](expr &&z)
                    {
                      return parametric_data_3d{std::move(std::get<0>(t)),
                                                std::move(std::get<1>(t)), std::move(z)};
                    });
          });
}

mark_type_2d transform_mark(ast::mark_type_2d mark)
{
  switch (mark)
  {
  case ast::mark_type_2d::impulses:
    return mark_type_2d::impulses;
  case ast::mark_type_2d::lines:
    return mark_type_2d::lines;
  case ast::mark_type_2d::points:
    return mark_type_2d::points;
  }
}

mark_type_3d transform_mark(ast::mark_type_3d mark)
{
  switch (mark)
  {
  case ast::mark_type_3d::lines:
    return mark_type_3d::lines;
  case ast::mark_type_3d::points:
    return mark_type_3d::points;
  }
}

line_type_spec transform_lts(const ast::line_type_spec &s)
{
  auto dt = s.dash_type.and_then(
      [](const dash_type_desc &dd) -> std::optional<dash_type>
      {
        if (std::holds_alternative<dash_type>(dd))
        {
          return std::get<dash_type>(dd);
        }
        else
        {
          return std::nullopt;
        }
      });
  return {.color = s.color, .width = s.width, .dash_type = dt};
}

line_type_desc transform_lt(const ast::line_type_desc &d)
{
  return std::visit(overload([](uint32_t i) -> line_type_desc { return i; },
                             [](const ast::line_type_spec &s) -> line_type_desc
                             { return transform_lts(s); }),
                    d);
}

std::expected<graph_desc_2d, std::string> validate(ast::graph_desc_2d &&graph)
{
  auto mark = transform_mark(graph.mark);
  auto data = std::visit([&](auto &&d) -> std::expected<data_source_2d, std::string>
                         { return validate(mark, std::move(d)); }, std::move(graph.data));
  return std::move(data).transform(
      [&](data_source_2d &&d)
      {
        return graph_desc_2d{.data = std::move(d),
                             .mark = mark,
                             .title = std::move(graph.title),
                             .line_type = transform_lt(graph.line_type)};
      });
}

std::expected<graph_desc_3d, std::string> validate(ast::graph_desc_3d &&graph)
{
  auto mark = transform_mark(graph.mark);
  auto data = std::visit([&](auto &&d) -> std::expected<data_source_3d, std::string>
                         { return validate(mark, std::move(d)); }, std::move(graph.data));
  return std::move(data).transform(
      [&](data_source_3d &&d)
      {
        return graph_desc_3d{.data = std::move(d),
                             .mark = mark,
                             .title = std::move(graph.title),
                             .line_type = transform_lt(graph.line_type)};
      });
}

std::expected<plot_command_2d, std::string> validate(ast::plot_command_2d &&plot)
{
  return validate_all(
             moving_range(plot.graphs)
             | std::views::transform([](ast::graph_desc_2d &&g) { return validate(std::move(g)); }))
      .transform(
          [&](std::vector<graph_desc_2d> &&gs)
          {
            return plot_command_2d{.graphs = std::move(gs),
                                   .x_range = plot.x_range,
                                   .y_range = plot.y_range,
                                   .t_range = plot.t_range};
          });
}

std::expected<plot_command_3d, std::string> validate(ast::plot_command_3d &&plot)
{
  return validate_all(
             moving_range(plot.graphs)
             | std::views::transform([](ast::graph_desc_3d &&g) { return validate(std::move(g)); }))
      .transform(
          [&](std::vector<graph_desc_3d> &&gs)
          {
            return plot_command_3d{.graphs = std::move(gs),
                                   .x_range = plot.x_range,
                                   .y_range = plot.y_range,
                                   .u_range = plot.u_range,
                                   .v_range = plot.v_range};
          });
}

} // namespace

namespace explot
{
std::expected<command, std::string> parse_command(const char *cmd)
{
  auto line = std::string_view(cmd);
  if (auto ast = parse_command_ast(line); ast)
  {
    return std::visit(overload([](ast::plot_command_2d &&cmd) -> std::expected<command, std::string>
                               { return validate(std::move(cmd)); },
                               [](ast::plot_command_3d &&cmd) -> std::expected<command, std::string>
                               { return validate(std::move(cmd)); },
                               [](set_command &&cmd) -> std::expected<command, std::string>
                               { return std::move(cmd); },
                               [](unset_command &&cmd) -> std::expected<command, std::string>
                               { return std::move(cmd); },
                               [](show_command &&cmd) -> std::expected<command, std::string>
                               { return std::move(cmd); },
                               [](quit_command &&cmd) -> std::expected<command, std::string>
                               { return std::move(cmd); },
                               [](ast::user_definition &&cmd) -> std::expected<command, std::string>
                               {
                                 const auto params =
                                     cmd.params
                                         .transform([](const std::vector<std::string> &v)
                                                    { return std::span{v}; })
                                         .value_or(std::span<const std::string>{});
                                 return validate_expression(std::move(cmd.body), params, false)
                                     .transform(
                                         [&](expr &&body)
                                         {
                                           return user_definition{.name = std::move(cmd.name),
                                                                  .params = std::move(cmd.params),
                                                                  .body = std::move(body)};
                                         });
                               }),
                      std::move(*ast));
  }
  else
  {
    return std::unexpected("unknown command");
  }
}

} // namespace explot
