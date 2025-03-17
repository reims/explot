#include "font_atlas.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <cmath>
#include <fmt/format.h>
#include <cassert>
#include <algorithm>
#include <fontconfig/fontconfig.h>
#include "program.hpp"
#include "rect.hpp"
#include <fstream>
#include <ios>

namespace
{
using namespace explot;

static constexpr auto string_vertex_shader_src = R"shader(#version 330 core
layout (location = 0) in vec2 uv_lower_bounds;
layout (location = 1) in vec2 uv_dimensions;
layout (location = 2) in vec2 screen_lower_bounds;
layout (location = 3) in vec2 screen_dimensions;
layout (location = 4) in vec2 pos;
out vec2 uv;

uniform vec2 offset;
uniform mat4 screen_to_clip;

void main()
{
  uv = uv_lower_bounds + pos * uv_dimensions;
  gl_Position = screen_to_clip * vec4(offset + screen_lower_bounds + pos * screen_dimensions, 0, 1);
}
)shader";

static constexpr auto atlas_vertex_shader_src = R"shader(#version 330 core
layout (location = 0) in vec2 pos;
out vec2 uv;

uniform vec2 offset;
uniform mat4 screen_to_clip;
uniform vec2 dims;

void main()
{
  uv = pos * dims;
  gl_Position = screen_to_clip * vec4(offset + dims * pos, 0, 1);
}
)shader";

static constexpr auto fragment_shader_src = R"shader(#version 330 core
in vec2 uv;

out vec4 FragColor;
uniform sampler2DRect tex;
uniform vec4 color;

void main()
{
  float alpha = texture(tex, floor(uv)).r;
  FragColor = vec4(color.rgb, alpha);
}
)shader";

auto make_string_program()
{
  return make_program(nullptr, string_vertex_shader_src, fragment_shader_src);
}

auto make_atlas_program()
{
  return make_program(nullptr, atlas_vertex_shader_src, fragment_shader_src);
}

static constexpr auto num_points = 6;
auto make_tex_vbo()
{
  auto vbo = explot::make_vbo();
  static constexpr auto data = std::array<float, 2 * num_points>{
      0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 2 * num_points * sizeof(float), data.data(), GL_STATIC_DRAW);
  return vbo;
}

template <typename T, typename F>
auto with_free(T *ptr, F f)
{
  return std::unique_ptr<T, F>(ptr, f);
}

std::pair<std::string, bool> preferred_monospace_font()
{
  auto conf = with_free(FcInitLoadConfigAndFonts(), FcConfigDestroy);
  auto pat = with_free(FcPatternCreate(), FcPatternDestroy);
  // reinterpret_cast seems necessary here because FcChar8 is unsigned
  // FcPatternAddString(pat.get(), "family", reinterpret_cast<const FcChar8 *>("monospace"));
  FcConfigSubstitute(conf.get(), pat.get(), FcMatchPattern);
  FcDefaultSubstitute(pat.get());
  FcResult result;
  auto font_patterns =
      with_free(FcFontSort(conf.get(), pat.get(), FcTrue, nullptr, &result), FcFontSetSortDestroy);
  if (!font_patterns || font_patterns->nfont == 0)
  {
    return {"", false};
  }
  FcValue formatVal;
  FcPatternGet(font_patterns->fonts[0], FC_FONTFORMAT, 0, &formatVal);
  auto format = std::string_view(static_cast<const char *>(formatVal.u.f));
  auto invert = format != "CFF"; // no idea if this is technically correct, but it works
  FcValue pathVal;
  FcPatternGet(font_patterns->fonts[0], FC_FILE, 0, &pathVal);
  return {std::string(static_cast<const char *>(pathVal.u.f)), invert};
}

static constexpr explot::rect uv_space{{0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};

auto init_ft()
{
  FT_Library lib;
  auto _ = FT_Init_FreeType(&lib);
  return with_free(lib, FT_Done_FreeType);
}

auto load_face(FT_Library lib, const char *path)
{
  FT_Face face;
  FT_New_Face(lib, path, 0, &face);
  return with_free(face, FT_Done_Face);
}

void write_ppm(const unsigned char *buffer, size_t dimx, size_t dimy, const char *filename)
{

  std::ofstream ofs(filename, std::ios::out | std::ios::binary);
  ofs << "P6\n" << dimx << ' ' << dimy << "\n255\n";

  for (auto j = 0u; j < dimy; ++j)
    for (auto i = 0u; i < dimx; ++i)
      ofs << static_cast<char>(buffer[j * dimx + i]) << static_cast<char>(0)
          << static_cast<char>(0);
}
} // namespace

namespace explot
{
using std::make_shared;

std::optional<font_atlas> make_font_atlas(std::string glyphs, int size)
{
  const auto [path, invert] = preferred_monospace_font();
  if (path.empty())
  {
    return std::nullopt;
  }
  auto ft = init_ft();
  if (ft)
  {
    auto font = load_face(ft.get(), path.c_str());
    if (font)
    {
      auto shapes = std::vector<glyph_handle>();
      shapes.reserve(glyphs.size());
      auto glyphs_data = std::vector<glyph_data>();
      glyphs_data.reserve(glyphs.size());
      //      auto glyphs_dimensions = make_unique_span<std::pair<int, int>>(glyphs.size());
      //      auto boundss = std::vector<msdfgen::Shape::Bounds>();
      auto width = 0;
      auto height = 0;
      auto error = FT_Set_Char_Size(font.get(), 0, size << 6, 0, 162);
      if (error)
      {
        fmt::println("error in set char size");
      }
      for (auto i = 0ULL; i < glyphs.size(); ++i)
      {
        auto glyph_index = FT_Get_Char_Index(font.get(), glyphs[i]);
        FT_Load_Glyph(font.get(), glyph_index, FT_LOAD_DEFAULT);
        FT_Glyph g;
        FT_Get_Glyph(font->glyph, &g);
        FT_Glyph_To_Bitmap(&g, FT_RENDER_MODE_NORMAL, 0, 1);
        shapes.emplace_back(g, FT_Done_Glyph);
        auto bitmap = (FT_BitmapGlyph)shapes[i].get();
        glyphs_data.emplace_back(glm::vec2(width, 0),
                                 glm::vec2(width + bitmap->bitmap.width, bitmap->bitmap.rows));
        width += bitmap->bitmap.width;
        height = std::max(height, static_cast<int>(bitmap->bitmap.rows));
      }
      auto tex_data = std::make_unique<unsigned char[]>(static_cast<std::size_t>(width)
                                                        * static_cast<std::size_t>(height));
      // std::fill_n(tex_data.get(), width * height, 0);
      auto dims = glm::vec2(width, height);
      auto start_column = 0u;
      for (auto i = 0ULL; i < glyphs.size(); ++i)
      {
        auto bitmap = (FT_BitmapGlyph)shapes[i].get();
        // fmt::println("width: {} pitch: {}", bitmap->bitmap.width, bitmap->bitmap.pitch);
        write_ppm(bitmap->bitmap.buffer, bitmap->bitmap.width, bitmap->bitmap.rows,
                  fmt::format("test_{}.ppm", glyphs[i]).c_str());
        for (auto row = 0u; row < bitmap->bitmap.rows; ++row)
        {
          std::memcpy(tex_data.get() + (row * width + start_column),
                      bitmap->bitmap.buffer
                          + (bitmap->bitmap.rows - row - 1) * bitmap->bitmap.width,
                      bitmap->bitmap.width);
        }
        start_column += bitmap->bitmap.width;
        // glyphs_data[i].uv_lb /= dims;
        // glyphs_data[i].uv_ub /= dims;
      }

      write_ppm(tex_data.get(), width, height, fmt::format("atlas_{}.ppm", glyphs.size()).c_str());

      auto tex = make_texture();
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_RECTANGLE, tex);
      const auto border_color = glm::vec4(0);
      glTexParameterfv(GL_TEXTURE_RECTANGLE, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border_color));
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE,
                   tex_data.get());
      glFinish();
      // glGenerateMipmap(GL_TEXTURE_RECTANGLE);
      auto quad = make_tex_vbo();
      auto vao = make_vao();
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, quad);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(0);
      return font_atlas{.ft = std::move(ft),
                        .glyphs = std::move(glyphs),
                        .ft_glyphs = std::move(shapes),
                        .data = std::move(glyphs_data),
                        .texture = std::move(tex),
                        .font = std::move(font),
                        .dims = {width, height},
                        .quad = std::move(quad),
                        .vao = std::move(vao),
                        .program = make_atlas_program()};
    }
  }
  fmt::print("failed make font atlas\n");
  return std::nullopt;
}

gl_string make_gl_string(const font_atlas &atlas, std::string_view str)
{
  // auto temp = std::make_unique<unsigned char[]>(static_cast<size_t>(atlas.dims.x *
  // atlas.dims.y)); glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RED, GL_UNSIGNED_BYTE, temp.get());
  // write_ppm(temp.get(), static_cast<size_t>(atlas.dims.x), static_cast<size_t>(atlas.dims.y),
  // fmt::format("atlas2_{}.ppm", atlas.glyphs.size()).c_str());
  auto vao = make_vao();
  glBindVertexArray(vao);
  auto uv_coordinates = std::make_unique<glm::vec2[]>(2 * str.size());
  auto screen_coordinates = std::make_unique<glm::vec2[]>(2 * str.size());
  auto width = 0.0f;
  auto y_lower_bound = std::numeric_limits<float>::max();
  auto y_upper_bound = std::numeric_limits<float>::lowest();
  auto previous = 0;
  auto has_kerning = FT_HAS_KERNING(atlas.font.get());
  for (auto i = 0ULL; i < str.size(); ++i)
  {
    // if (std::isspace(str[i]))
    // {
    //   width += space_width;
    //   continue;
    // }
    auto idx = static_cast<std::size_t>(
        std::distance(std::begin(atlas.glyphs),
                      std::find(std::begin(atlas.glyphs), std::end(atlas.glyphs), str[i])));
    auto glyph_index = FT_Get_Char_Index(atlas.font.get(), str[i]);
    assert(idx < atlas.glyphs.size() && glyph_index > 0);
    auto bitmap = (FT_BitmapGlyph)atlas.ft_glyphs[idx].get();
    const auto &uv_lower_bounds = atlas.data[idx].uv_lb;
    const auto uv_dimensions = atlas.data[idx].uv_ub - atlas.data[idx].uv_lb;
    static_assert(sizeof(glm::vec2) == 2 * sizeof(float));
    std::memcpy(&uv_coordinates[2 * i], &uv_lower_bounds, sizeof(glm::vec2));
    std::memcpy(&uv_coordinates[2 * i + 1], &uv_dimensions, sizeof(glm::vec2));
    if (i > 0 && has_kerning)
    {
      FT_Vector kerning = {0, 0};
      FT_Get_Kerning(atlas.font.get(), previous, glyph_index, FT_KERNING_DEFAULT, &kerning);
      width += (kerning.x >> 6);
    }
    const auto screen_lower_bounds =
        glm::vec2(bitmap->left, bitmap->top - bitmap->bitmap.rows) + glm::vec2(width, 0.0f);
    const auto screen_dimensions = glm::vec2(bitmap->bitmap.width, bitmap->bitmap.rows);
    width += bitmap->root.advance.x >> 16;
    y_lower_bound = std::min(y_lower_bound, screen_lower_bounds.y);
    y_upper_bound = std::max(y_upper_bound, screen_lower_bounds.y + screen_dimensions.y);
    std::memcpy(&screen_coordinates[2 * i], &screen_lower_bounds, sizeof(glm::vec2));
    std::memcpy(&screen_coordinates[2 * i + 1], &screen_dimensions, sizeof(glm::vec2));
    previous = glyph_index;
  }
  auto uv_vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, uv_vbo);
  glBufferData(GL_ARRAY_BUFFER, 4 * str.size() * sizeof(float), uv_coordinates.get(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribDivisor(0, 1);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
  glVertexAttribDivisor(1, 1);
  glEnableVertexAttribArray(1);
  auto screen_vbo = make_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, screen_vbo);
  glBufferData(GL_ARRAY_BUFFER, 4 * str.size() * sizeof(float), screen_coordinates.get(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribDivisor(2, 1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
  glVertexAttribDivisor(3, 1);
  glEnableVertexAttribArray(3);
  auto tex_vbo = make_tex_vbo();
  glBindBuffer(GL_ARRAY_BUFFER, tex_vbo);
  glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(4);
  return gl_string{.size = static_cast<std::uint32_t>(str.size()),
                   .vao = std::move(vao),
                   .uv_coordinates = std::move(uv_vbo),
                   .screen_coordinates = std::move(screen_vbo),
                   .tex_vbo = std::move(tex_vbo),
                   .texture = atlas.texture,
                   .program = make_string_program(),
                   .lower_bounds = glm::vec2{0.0f, y_lower_bound},
                   .upper_bounds = glm::vec2{width, y_upper_bound}};
}

void draw(const gl_string &str, const glm::mat4 &screen_to_clip, const glm::vec2 &offset,
          const glm::vec4 &color, const glm::vec2 &anchor)
{
  // fmt::print("draw string\n");
  glBindVertexArray(str.vao);
  glBindTexture(GL_TEXTURE_RECTANGLE, str.texture);
  glUseProgram(str.program);
  // a * (u - l) = au - al
  const auto offset_with_anchor =
      glm::floor(offset - anchor * str.upper_bounds - (1.0f - anchor) * str.lower_bounds);
  glUniformMatrix4fv(glGetUniformLocation(str.program, "screen_to_clip"), 1, GL_FALSE,
                     glm::value_ptr(screen_to_clip));
  glUniform2fv(glGetUniformLocation(str.program, "offset"), 1, glm::value_ptr(offset_with_anchor));
  glUniform4fv(glGetUniformLocation(str.program, "color"), 1, glm::value_ptr(color));
  glDrawArraysInstanced(GL_TRIANGLES, 0, num_points, str.size);
}

void draw(const font_atlas &atlas, const glm::mat4 &screen_to_clip, const glm::vec2 &offset)
{
  auto color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
  glBindVertexArray(atlas.vao);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_RECTANGLE, atlas.texture);
  glUseProgram(atlas.program);
  glUniform1i(glGetUniformLocation(atlas.program, "tex"), 0);
  glUniformMatrix4fv(glGetUniformLocation(atlas.program, "screen_to_clip"), 1, GL_FALSE,
                     glm::value_ptr(screen_to_clip));
  glUniform2fv(glGetUniformLocation(atlas.program, "offset"), 1, glm::value_ptr(offset));
  glUniform2fv(glGetUniformLocation(atlas.program, "dims"), 1, glm::value_ptr(atlas.dims));
  glUniform4fv(glGetUniformLocation(atlas.program, "color"), 1, glm::value_ptr(color));
  glDrawArrays(GL_TRIANGLES, 0, num_points);
}
} // namespace explot
