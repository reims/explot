#include "user_definitions.hpp"
#include <algorithm>
#include <ranges>
namespace
{
using namespace explot;

std::vector<user_definition> definitions;
} // namespace

namespace explot
{
void add_definition(user_definition def) { definitions.push_back(std::move(def)); }

std::optional<uint32_t> find_user_function(std::string_view name)
{
  auto rdefs = std::views::reverse(definitions);
  auto it = std::ranges::find_if(rdefs, [&](const user_definition &d)
                                 { return d.name == name && d.params.has_value(); });
  if (it != rdefs.end())
  {
    return std::distance(definitions.begin(), it.base()) - 1;
  }
  else
  {
    return std::nullopt;
  }
}

std::optional<uint32_t> find_user_variable(std::string_view name)
{
  auto rdefs = std::views::reverse(definitions);
  auto it = std::ranges::find_if(rdefs, [&](const user_definition &d)
                                 { return d.name == name && !d.params.has_value(); });
  if (it != rdefs.end())
  {
    return std::distance(definitions.begin(), it.base()) - 1;
  }
  else
  {
    return std::nullopt;
  }
}

const user_definition &get_definition(uint32_t idx)
{
  assert(idx < definitions.size());
  return definitions[idx];
}

} // namespace explot
