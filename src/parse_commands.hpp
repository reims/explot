#pragma once
#include "commands.hpp"
#include <expected>

namespace explot
{
std::expected<command, std::string> parse_command(const char *cmd);
} // namespace explot
