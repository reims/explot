#include "line_type.hpp"
#include "settings.hpp"
#include "overload.hpp"

namespace
{
using namespace explot;
template <typename T>
std::vector<line_type> resolve_line_types_(std::span<const T> graphs)
{
  auto result = std::vector<line_type>();
  result.reserve(graphs.size());
  auto idx = 1u;
  for (const auto &g : graphs)
  {
    auto lt = std::visit(overload(
                             [&](uint32_t ref)
                             {
                               idx = ref;
                               return settings::line_type_by_index(idx);
                             },
                             [&](line_type_spec lt)
                             {
                               const auto &ref = settings::line_type_by_index(idx);
                               auto dt = lt.dash_type.and_then(
                                   [](dash_type_desc dd) -> std::optional<dash_type>
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
                               return line_type{.width = lt.width.value_or(ref.width),
                                                .color = lt.color.value_or(ref.color),
                                                .dash_type = dt};
                             }),
                         g.line_type);
    result.push_back(lt);
    ++idx;
  }
  return result;
}
} // namespace

namespace explot
{
std::vector<line_type> resolve_line_types(std::span<const graph_desc_2d> graphs)
{
  return resolve_line_types_(graphs);
}

std::vector<line_type> resolve_line_types(std::span<const graph_desc_3d> graphs)
{
  return resolve_line_types_(graphs);
}

} // namespace explot
