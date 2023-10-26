#pragma once

#include "gl-handle.hpp"
#include <glm/glm.hpp>
#include <variant>
#include <span>

namespace explot
{
program_handle make_program_with_varying(const char *shader_src, const char *varying);

program_handle make_program(const char *geometry_shader_src, const char *vertex_shader_src,
                            const char *fragment_shader_src);

using uniform_value = std::variant<float, glm::vec4, glm::vec2, glm::mat4>;
using uniform = std::pair<const char *, uniform_value>;

void set_uniforms(gl_id program, std::span<const uniform> uniforms);

} // namespace explot
