#pragma once

#include <cstdint>
#include <string_view>
#include <optional>
#include "commands.hpp"

namespace explot
{
std::optional<uint32_t> find_user_function(std::string_view name);
std::optional<uint32_t> find_user_variable(std::string_view name);

void add_definition(user_definition def);
const user_definition &get_definition(uint32_t idx);
} // namespace explot
