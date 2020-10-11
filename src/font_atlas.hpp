#pragma once
#include <memory>
#include <span>
#include "gl-handle.hpp"
#include <string>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <optional>
#include "rect.hpp"
#include "unique_span.hpp"

namespace explot
{
struct glyph_data final
{
  glm::vec2 uv_lb;
  glm::vec2 uv_ub;
  glm::vec2 lower_bounds;
  glm::vec2 upper_bounds;
};

struct font_atlas final
{
  std::string glyphs;
  unique_span<glyph_data> data;
  std::shared_ptr<texture_handle> texture;
};

std::optional<font_atlas> make_font_atlas(std::string glyphs);

struct gl_string final
{
  std::uint32_t size;
  vao_handle vao;
  vbo_handle uv_coordinates;
  vbo_handle screen_coordinates;
  vbo_handle tex_vbo;
  std::shared_ptr<texture_handle> texture;
  program_handle program;
  glm::vec2 lower_bounds;
  glm::vec2 upper_bounds;
};

gl_string make_gl_string(const font_atlas &atlas, std::string_view str);
void draw(const gl_string &str, const glm::mat4 &screen_to_clip, const glm::vec2 &offset,
          const glm::vec4 &color, float scale = 1.0f, const glm::vec2 &anchor = {0.0f, 0.0f});
} // namespace explot
