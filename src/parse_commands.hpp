#pragma once
#include <optional>
#include "commands.hpp"

namespace explot
{
std::optional<command> parse_command(const char *cmd);
}
