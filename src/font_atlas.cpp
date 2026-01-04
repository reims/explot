#include "font_atlas.hpp"
#include "gl-handle.hpp"
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
#include <array>
#include <unordered_map>

namespace
{
using namespace explot;

static constexpr auto string_vertex_shader_src = R"shader(#version 430 core
layout (location = 0) in vec2 uv_lower_bounds;
layout (location = 1) in vec2 uv_dimensions;
layout (location = 2) in vec2 screen_lower_bounds;
layout (location = 3) in vec2 screen_dimensions;
layout (location = 4) in vec2 pos;
out vec2 uv;

uniform vec3 offset;

uniform mat4 screen_to_clip;


void main()
{
  uv = uv_lower_bounds + pos * uv_dimensions;
  gl_Position = screen_to_clip * vec4(offset.xy + screen_lower_bounds + pos * screen_dimensions, offset.z, 1);
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

std::string preferred_monospace_font()
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
  FcValue pathVal;
  FcPatternGet(font_patterns->fonts[0], FC_FILE, 0, &pathVal);
  return std::string(static_cast<const char *>(pathVal.u.f));
}

auto init_ft()
{
  FT_Library lib;
  auto _ = FT_Init_FreeType(&lib);
  return freetype_handle(lib);
}

auto load_face(FT_Library lib, const char *path)
{
  FT_Face face;
  FT_New_Face(lib, path, 0, &face);
  return font_handle(face);
}

/*
void write_ppm(const unsigned char *buffer, size_t dimx, size_t dimy, const char *filename)
{

std::ofstream ofs(filename, std::ios::out | std::ios::binary);
ofs << "P6\n" << dimx << ' ' << dimy << "\n255\n";

for (auto j = 0u; j < dimy; ++j)
  for (auto i = 0u; i < dimx; ++i)
    ofs << static_cast<char>(buffer[j * dimx + i]) << static_cast<char>(0)
        << static_cast<char>(0);
}

*/

struct ft_state_t
{
  freetype_handle ft = {};
  font_handle font = {};
  std::unordered_map<char, glyph_handle> glyphs = {};
  int size = {};
} ft_state;

} // namespace

namespace explot
{

std::expected<void, std::string> init_freetype(int size)
{
  if (ft_state.ft)
  {
    return {};
  }

  ft_state.ft = init_ft();
  if (!ft_state.ft)
  {
    return std::unexpected("Failed to initialize freetype.");
  }

  const auto path = preferred_monospace_font();
  if (path.empty())
  {
    return std::unexpected("Could not find any font.");
  }

  ft_state.font = load_face(ft_state.ft.get(), path.c_str());
  if (!ft_state.font)
  {
    return std::unexpected("Failed to load font");
  }

  ft_state.size = size;

  return {};
}

font_atlas make_font_atlas(std::string glyphs)
{
  assert(ft_state.ft);
  auto font = ft_state.font.get();
  assert(font);
  auto glyphs_data = std::vector<glyph_data>();
  glyphs_data.reserve(glyphs.size());
  auto width = 0;
  auto height = 0;
  auto error = FT_Set_Char_Size(font, 0, ft_state.size << 6, 0, 96);
  if (error)
  {
    fmt::println("error in set char size");
  }
  for (auto i = 0ULL; i < glyphs.size(); ++i)
  {
    FT_Glyph g;
    if (auto stored_glyph = ft_state.glyphs.find(glyphs[i]); stored_glyph != ft_state.glyphs.end())
    {
      g = stored_glyph->second.get();
    }
    else
    {
      auto glyph_index = FT_Get_Char_Index(font, FT_ULong(glyphs[i]));
      assert(glyph_index > 0);
      FT_Load_Glyph(font, glyph_index, FT_LOAD_DEFAULT);
      FT_Get_Glyph(font->glyph, &g);
      FT_Glyph_To_Bitmap(&g, FT_RENDER_MODE_NORMAL, 0, 1);
      ft_state.glyphs[glyphs[i]] = glyph_handle(g);
    }

    auto bitmap = (FT_BitmapGlyph)g;
    glyphs_data.emplace_back(glm::vec2(width, 0),
                             glm::vec2(width + int(bitmap->bitmap.width), bitmap->bitmap.rows));

    width += bitmap->bitmap.width;
    height = std::max(height, static_cast<int>(bitmap->bitmap.rows));
  }
  // rendered text is garbled, if width is not a multiple of 4
  // I guess that rounding errors are the reason.
  // This feels more like a workaround than a fix though
  if (width & 3)
  {
    width += 4 - (width & 3);
  }
  auto tex_data = std::make_unique<unsigned char[]>(static_cast<std::size_t>(width)
                                                    * static_cast<std::size_t>(height));
  auto dims = glm::vec2(width, height);
  auto start_column = 0u;
  for (auto i = 0ULL; i < glyphs.size(); ++i)
  {
    auto bitmap = (FT_BitmapGlyph)ft_state.glyphs[glyphs[i]].get();
    for (auto row = 0u; row < bitmap->bitmap.rows; ++row)
    {
      std::memcpy(tex_data.get() + (int(row) * width + int(start_column)),
                  bitmap->bitmap.buffer + (bitmap->bitmap.rows - row - 1) * bitmap->bitmap.width,
                  bitmap->bitmap.width);
    }
    start_column += bitmap->bitmap.width;
  }

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

  auto quad = make_tex_vbo();
  auto vao = make_vao();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, quad);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  return font_atlas{.glyphs = std::move(glyphs),
                    .data = std::move(glyphs_data),
                    .texture = std::move(tex),
                    .dims = {width, height},
                    .quad = std::move(quad),
                    .vao = std::move(vao),
                    .program = make_atlas_program()};
}

gl_string::gl_string(const font_atlas &atlas, std::string_view str, const glm::vec4 &color)
    : size(static_cast<uint32_t>(str.size())), vao(make_vao()), uv_coordinates(make_vbo()),
      screen_coordinates(make_vbo()), tex_vbo(make_tex_vbo()), texture(atlas.texture),
      program(make_string_program())
{
  assert(ft_state.ft);
  glBindVertexArray(vao);
  auto uv_coords = std::make_unique<glm::vec2[]>(2 * str.size());
  auto screen_coords = std::make_unique<glm::vec2[]>(2 * str.size());
  auto width = 0.0f;
  auto y_lower_bound = std::numeric_limits<float>::max();
  auto y_upper_bound = std::numeric_limits<float>::lowest();
  auto previous = 0u;
  auto font = ft_state.font.get();
  auto has_kerning = FT_HAS_KERNING(font);
  for (auto i = 0ULL; i < str.size(); ++i)
  {
    auto idx = static_cast<std::size_t>(
        std::distance(std::begin(atlas.glyphs),
                      std::find(std::begin(atlas.glyphs), std::end(atlas.glyphs), str[i])));
    auto glyph_index = FT_Get_Char_Index(font, FT_ULong(str[i]));
    assert(idx < atlas.glyphs.size());
    assert(glyph_index > 0);
    auto bitmap = (FT_BitmapGlyph)ft_state.glyphs[str[i]].get();
    const auto &uv_lower_bounds = atlas.data[idx].uv_lb;
    const auto uv_dimensions = atlas.data[idx].uv_ub - atlas.data[idx].uv_lb;
    static_assert(sizeof(glm::vec2) == 2 * sizeof(float));
    std::memcpy(&uv_coords[2 * i], &uv_lower_bounds, sizeof(glm::vec2));
    std::memcpy(&uv_coords[2 * i + 1], &uv_dimensions, sizeof(glm::vec2));
    if (i > 0 && has_kerning)
    {
      FT_Vector kerning = {0, 0};
      FT_Get_Kerning(font, previous, glyph_index, FT_KERNING_DEFAULT, &kerning);
      width += static_cast<float>(kerning.x >> 6);
    }
    const auto screen_lower_bounds =
        glm::vec2(bitmap->left, bitmap->top - static_cast<int>(bitmap->bitmap.rows))
        + glm::vec2(width, 0.0f);
    const auto screen_dimensions = glm::vec2(bitmap->bitmap.width, bitmap->bitmap.rows);
    width += static_cast<float>(bitmap->root.advance.x >> 16);
    y_lower_bound = std::min(y_lower_bound, screen_lower_bounds.y);
    y_upper_bound = std::max(y_upper_bound, screen_lower_bounds.y + screen_dimensions.y);
    std::memcpy(&screen_coords[2 * i], &screen_lower_bounds, sizeof(glm::vec2));
    std::memcpy(&screen_coords[2 * i + 1], &screen_dimensions, sizeof(glm::vec2));
    previous = glyph_index;
  }
  glBindBuffer(GL_ARRAY_BUFFER, uv_coordinates);
  glBufferData(GL_ARRAY_BUFFER, 4 * str.size() * sizeof(float), uv_coords.get(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribDivisor(0, 1);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
  glVertexAttribDivisor(1, 1);
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, screen_coordinates);
  glBufferData(GL_ARRAY_BUFFER, 4 * str.size() * sizeof(float), screen_coords.get(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
  glVertexAttribDivisor(2, 1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
  glVertexAttribDivisor(3, 1);
  glEnableVertexAttribArray(3);
  glBindBuffer(GL_ARRAY_BUFFER, tex_vbo);
  glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(4);
  lower_bounds = glm::vec2(0.0f, y_lower_bound);
  upper_bounds = glm::vec2(width, y_upper_bound);
  glUseProgram(program);
  glUniform4fv(glGetUniformLocation(program, "color"), 1, glm::value_ptr(color));
}

void draw(const gl_string &str)
{
  // fmt::print("draw string\n");
  glBindVertexArray(str.vao);
  glBindTexture(GL_TEXTURE_RECTANGLE, str.texture);
  glUseProgram(str.program);
  glDrawArraysInstanced(GL_TRIANGLES, 0, num_points, str.size);
}

void update(const gl_string &str, const glm::vec3 &offset, const glm::vec2 &anchor,
            const glm::mat4 &screen_to_clip)
{
  const auto offset2 = glm::vec2(offset.x, offset.y);
  const auto offset_with_anchor = glm::vec3(
      glm::floor(offset2 - anchor * str.upper_bounds - (1.0f - anchor) * str.lower_bounds),
      offset.z);
  glUseProgram(str.program);
  glUniformMatrix4fv(glGetUniformLocation(str.program, "screen_to_clip"), 1, GL_FALSE,
                     glm::value_ptr(screen_to_clip));
  glUniform3fv(glGetUniformLocation(str.program, "offset"), 1, glm::value_ptr(offset_with_anchor));
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
