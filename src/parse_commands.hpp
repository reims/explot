#pragma once
#include <optional>
#include "commands.hpp"
#include <string_view>

namespace explot
{
std::optional<command> parse_command(const char *cmd);
std::optional<range_setting> parse_range_setting(std::string_view s);
} // namespace explot
