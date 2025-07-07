#pragma once
#include <memory>
#include "gl-handle.hpp"
#include <string>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <optional>
#include <ft2build.h>
#include <vector>
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace explot
{
struct glyph_data final
{
  glm::vec2 uv_lb;
  glm::vec2 uv_ub;
};

// With unique_ptr<T, void (*)(T*)>, the function ptr is a member variable of unique_ptr. This
// doubles the size of unique_ptr and the function call is not static anymore. This wrapper class
// makes the function ptr a compile-time constant
template <auto f>
struct func_wrapper
{
  template <typename T>
  void operator()(T &&t)
  {
    f(std::forward<T>(t));
  }
};

using ft_library = std::remove_pointer_t<FT_Library>;
using ft_font = std::remove_pointer_t<FT_Face>;
using ft_glyph = std::remove_pointer_t<FT_Glyph>;
using freetype_handle = std::unique_ptr<ft_library, func_wrapper<FT_Done_FreeType>>;
using font_handle = std::unique_ptr<ft_font, func_wrapper<FT_Done_Face>>;
using glyph_handle = std::unique_ptr<ft_glyph, func_wrapper<FT_Done_Glyph>>;

struct font_atlas final
{
  freetype_handle ft;
  std::string glyphs;
  std::vector<glyph_handle> ft_glyphs;
  std::vector<glyph_data> data;
  texture_handle texture;
  font_handle font;
  glm::vec2 dims;
  vbo_handle quad;
  vao_handle vao;
  program_handle program;
};

std::optional<font_atlas> make_font_atlas(std::string glyphs, int size);

struct gl_string final
{
  gl_string(const font_atlas &atlas, std::string_view str, const glm::vec4 &color);

  std::uint32_t size;
  vao_handle vao;
  vbo_handle uv_coordinates;
  vbo_handle screen_coordinates;
  vbo_handle tex_vbo;
  gl_id texture;
  program_handle program;
  glm::vec2 lower_bounds;
  glm::vec2 upper_bounds;
};
void update(const gl_string &str, const glm::vec2 &offset, const glm::vec2 &anchor = {0.0f, 0.0f});
void draw(const gl_string &str);
void draw(const font_atlas &atlas, const glm::mat4 &screen_to_clip, const glm::vec2 &offset);
} // namespace explot
