#pragma once

#include "gl-handle.hpp"

namespace explot
{
program_handle make_program_with_varying(const char *shader_src, const char *varying);

program_handle make_program(const char *geometry_shader_src, const char *vertex_shader_src,
                            const char *fragment_shader_src);
} // namespace explot
