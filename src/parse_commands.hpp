#pragma once
#include "commands.hpp"
#include <expected>

namespace explot
{
std::expected<command, std::string> parse_command(std::string_view line);
} // namespace explot
