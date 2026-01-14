#pragma once

#include "gl-handle.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <variant>
#include <span>

namespace explot
{
program_handle make_program_with_varying(const char *shader_src, const char *varying);
program_handle make_program_with_varying(const char *shader_src, std::span<const char *> varyings);
program_handle make_program(const char *geometry_shader_src, const char *vertex_shader_src,
                            const char *fragment_shader_src);

program_handle make_compute_program(const char *compute_shader_src);

using uniform_value = std::variant<float, glm::vec4, glm::vec3, glm::vec2, glm::mat4, uint32_t,
                                   std::span<const float>, std::span<const uint32_t>>;
using uniform = std::pair<const char *, uniform_value>;

void set_uniforms(gl_id program, std::span<const uniform> uniforms);

struct uniform_bindings_2d
{
  uint32_t phase_to_screen = 0;
  uint32_t screen_to_clip = 1;
};

struct uniform_bindings_3d
{
  uint32_t phase_to_clip = 0;
  uint32_t screen_to_clip = 1;
  uint32_t clip_to_screen = 2;
};

struct transforms_2d
{
  glm::mat4 phase_to_screen;
  glm::mat4 screen_to_clip;
};

struct transforms_3d
{
  glm::mat4 phase_to_clip;
  glm::mat4 screen_to_clip;
  glm::mat4 clip_to_screen;
};

void set_transforms(gl_id program, const transforms_2d &transforms);
void set_transforms(gl_id program, const transforms_3d &transforms);
} // namespace explot
