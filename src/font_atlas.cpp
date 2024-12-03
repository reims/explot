#include "font_atlas.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include "msdfgen.h"
#include <ext/import-font.h>
#include <cmath>
#include <fmt/format.h>
#include <cassert>
#include <algorithm>
#include <fontconfig/fontconfig.h>
#include "program.hpp"

namespace
{

static constexpr auto string_vertex_shader_src = R"shader(#version 330 core
layout (location = 0) in vec2 uv_lower_bounds;
layout (location = 1) in vec2 uv_dimensions;
layout (location = 2) in vec2 screen_lower_bounds;
layout (location = 3) in vec2 screen_dimensions;
layout (location = 4) in vec2 pos;
out vec2 uv;

uniform vec2 offset;
uniform mat4 screen_to_clip;
uniform float scale;

void main()
{
  uv = uv_lower_bounds + pos * uv_dimensions;
  gl_Position = screen_to_clip * vec4(offset + scale * (screen_lower_bounds + pos * screen_dimensions), 0, 1);
}
)shader";

static constexpr auto fragment_shader_src = R"shader(#version 330 core
in vec2 uv;

out vec4 FragColor;
uniform sampler2D tex;
uniform float scale;
uniform vec4 color;
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
  vec4 msdf = texture(tex, uv);
  float sd = median(msdf.x, msdf.y, msdf.z);
  float s = 4.0 * scale;
  float dist = clamp(s * (sd - 0.5) + 0.5, 0, 1);
  float alpha = max(0.0, dist);
  FragColor = vec4(color.xyz, alpha);
}
)shader";

auto make_string_program()
{
  return explot::make_program(nullptr, string_vertex_shader_src, fragment_shader_src);
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
  FcPatternAddString(pat.get(), "family", reinterpret_cast<const FcChar8 *>("monospace"));
  FcConfigSubstitute(conf.get(), pat.get(), FcMatchPattern);
  FcDefaultSubstitute(pat.get());
  FcResult result;
  auto font_patterns =
      with_free(FcFontSort(conf.get(), pat.get(), FcTrue, nullptr, &result), FcFontSetSortDestroy);
  if (!font_patterns || font_patterns->nfont == 0)
  {
    return "";
  }
  FcValue val;
  FcPatternGet(font_patterns->fonts[0], FC_FILE, 0, &val);
  return std::string(static_cast<const char *>(val.u.f));
}

static constexpr explot::rect uv_space{{0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
} // namespace

namespace explot
{
using std::make_shared;

std::optional<font_atlas> make_font_atlas(std::string glyphs)
{
  const auto path = preferred_monospace_font();
  if (path.empty())
  {
    return std::nullopt;
  }
  auto ft = with_free(msdfgen::initializeFreetype(), msdfgen::deinitializeFreetype);
  if (ft)
  {
    auto font = with_free(msdfgen::loadFont(ft.get(), path.c_str()), msdfgen::destroyFont);
    if (font)
    {
      auto shapes = make_unique_span<msdfgen::Shape>(glyphs.size());
      auto glyphs_data = make_unique_span<glyph_data>(glyphs.size());
      auto glyphs_dimensions = make_unique_span<std::pair<int, int>>(glyphs.size());
      auto width = 0;
      auto height = 0;
      // fmt::print("start loading glyphs\n");
      for (auto i = 0ULL; i < glyphs.size(); ++i)
      {
        msdfgen::loadGlyph(shapes[i], font.get(), static_cast<msdfgen::unicode_t>(glyphs[i]));
        msdfgen::edgeColoringSimple(shapes[i], 3);
        const auto bounds = shapes[i].getBounds(1.0);
        const auto glyph_width = static_cast<int>(std::ceil(bounds.r - bounds.l));
        const auto glyph_height = static_cast<int>(std::ceil(bounds.t - bounds.b));
        glyphs_data[i].lower_bounds = {bounds.l, bounds.b};
        glyphs_data[i].upper_bounds = {bounds.r, bounds.t};
        glyphs_data[i].uv_lb = {width, 0.0f};
        glyphs_data[i].uv_ub = {width + glyph_width, glyph_height};
        glyphs_dimensions[i].first = glyph_width;
        glyphs_dimensions[i].second = glyph_height;
        width += glyph_width;
        height = std::max(height, glyph_height);
      }
      // fmt::print("start making tex data\n");
      auto tex_data = std::make_unique<float[]>(3 * static_cast<std::size_t>(width)
                                                * static_cast<std::size_t>(height));
      std::fill_n(tex_data.get(), 3 * width * height, 1.0f);
      // fmt::print("texture dims = {} x {} = {}\n", width, height, width * height);
      for (auto i = 0ULL; i < glyphs.size(); ++i)
      {
        auto msdf =
            msdfgen::Bitmap<float, 3>(glyphs_dimensions[i].first, glyphs_dimensions[i].second);
        msdfgen::generateMSDF(
            msdf, shapes[i], 4.0, 1.0,
            msdfgen::Vector2(-glyphs_data[i].lower_bounds.x, -glyphs_data[i].lower_bounds.y));
        // fmt::print("generated msdf for {}\n", glyphs[i]);
        auto start_row = static_cast<int>(glyphs_data[i].uv_lb.y);
        auto start_column = static_cast<int>(glyphs_data[i].uv_lb.x);
        auto end_row = static_cast<int>(glyphs_data[i].uv_ub.y);
        auto columns = static_cast<int>(glyphs_data[i].uv_ub.x) - start_column;
        for (auto row = start_row; row < end_row; ++row)
        {
          // fmt::print("copying row {} start index = {}, start in msdf = {}, columns = {}\n", row,
          //            row * width + start_column, row * glyphs_dimensions[i].first, columns);
          std::memcpy(tex_data.get() + 3 * (row * width + start_column),
                      static_cast<const float *>(msdf) + 3 * row * glyphs_dimensions[i].first,
                      3 * static_cast<std::size_t>(columns) * sizeof(float));
        }
        // fmt::print("memcpied data for {}\n", glyphs[i]);
        glyphs_data[i].uv_lb =
            (glyphs_data[i].uv_lb + glm::vec2(0.5f, 0.5f)) / glm::vec2(width, height);
        glyphs_data[i].uv_ub =
            (glyphs_data[i].uv_ub - glm::vec2(0.5f, 0.5f)) / glm::vec2(width, height);
        ;
      }
      // fmt::print("start make texture\n");
      auto tex = make_texture();
      glBindTexture(GL_TEXTURE_2D, tex);
      const auto border_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
      glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border_color));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, tex_data.get());
      glGenerateMipmap(GL_TEXTURE_2D);
      return font_atlas{.glyphs = std::move(glyphs),
                        .data = std::move(glyphs_data),
                        .texture = make_shared<texture_handle>(std::move(tex))};
    }
  }
  fmt::print("failed make font atlas\n");
  return std::nullopt;
}

gl_string make_gl_string(const font_atlas &atlas, std::string_view str)
{
  auto vao = make_vao();
  glBindVertexArray(vao);
  auto uv_coordinates = std::make_unique<glm::vec2[]>(2 * str.size());
  auto screen_coordinates = std::make_unique<glm::vec2[]>(2 * str.size());
  auto width = 0.0f;
  auto y_lower_bound = std::numeric_limits<float>::max();
  auto y_upper_bound = std::numeric_limits<float>::lowest();
  for (auto i = 0ULL; i < str.size(); ++i)
  {
    if (std::isspace(str[i]))
    {
      width += 5;
      continue;
    }
    auto idx = static_cast<std::size_t>(
        std::distance(std::begin(atlas.glyphs),
                      std::find(std::begin(atlas.glyphs), std::end(atlas.glyphs), str[i])));
    assert(idx < atlas.glyphs.size());
    const auto &uv_lower_bounds = atlas.data[idx].uv_lb;
    const auto uv_dimensions = atlas.data[idx].uv_ub - atlas.data[idx].uv_lb;
    static_assert(sizeof(glm::vec2) == 2 * sizeof(float));
    std::memcpy(&uv_coordinates[2 * i], &uv_lower_bounds, sizeof(glm::vec2));
    std::memcpy(&uv_coordinates[2 * i + 1], &uv_dimensions, sizeof(glm::vec2));
    const auto screen_lower_bounds = atlas.data[idx].lower_bounds + glm::vec2(width, 0.0f);
    const auto screen_dimensions = atlas.data[idx].upper_bounds - atlas.data[idx].lower_bounds;
    width += atlas.data[idx].upper_bounds.x;
    y_lower_bound = std::min(y_lower_bound, atlas.data[idx].lower_bounds.y);
    y_upper_bound = std::max(y_upper_bound, atlas.data[idx].upper_bounds.y);
    std::memcpy(&screen_coordinates[2 * i], &screen_lower_bounds, sizeof(glm::vec2));
    std::memcpy(&screen_coordinates[2 * i + 1], &screen_dimensions, sizeof(glm::vec2));
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
          const glm::vec4 &color, float scale, const glm::vec2 &anchor)
{
  // fmt::print("draw string\n");
  glBindVertexArray(str.vao);
  glBindTexture(GL_TEXTURE_2D, *str.texture);
  glUseProgram(str.program);
  const auto offset_with_anchor =
      offset - scale * anchor * str.upper_bounds - scale * (1.0f - anchor) * str.lower_bounds;
  glUniformMatrix4fv(glGetUniformLocation(str.program, "screen_to_clip"), 1, GL_FALSE,
                     glm::value_ptr(screen_to_clip));
  glUniform2fv(glGetUniformLocation(str.program, "offset"), 1, glm::value_ptr(offset_with_anchor));
  glUniform1f(glGetUniformLocation(str.program, "scale"), scale);
  glUniform4fv(glGetUniformLocation(str.program, "color"), 1, glm::value_ptr(color));
  glDrawArraysInstanced(GL_TRIANGLES, 0, num_points, str.size);
}

} // namespace explot
