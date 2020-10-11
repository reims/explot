#include "fps_counter.hpp"

#include <array>
#include <glm/gtc/type_ptr.hpp>
#include "msdfgen.h"
#include "ext/import-font.h"
#include <string_view>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/gtx/string_cast.hpp>
#include <fmt/format.h>
#include <cmath>

namespace
{
static constexpr auto vertex_shader_src = R"shader(#version 330 core

layout (location = 0) in vec2 pos;

out vec2 uv;

uniform mat4 uv_to_clip;

void main()
{
  gl_Position = uv_to_clip * vec4(pos, 0, 1);
  uv = pos;
}
)shader";

static constexpr auto fragment_shader_src = R"shader(#version 330 core
in vec2 uv;

out vec4 FragColor;
uniform sampler2D tex;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
  vec4 msdf = texture(tex, uv);
  float sd = median(msdf.x, msdf.y, msdf.z);
  float scale = 4.0;
  float dist = clamp(scale * (sd - 0.5) + 0.5, 0, 1);
  FragColor = vec4(1.0, 0.0, 0.0, 1 - dist);
}
)shader";

auto make_font_texture(const char *path, char glyph)
{
  auto ft = msdfgen::initializeFreetype();
  if (ft)
  {
    auto font = msdfgen::loadFont(ft, path);
    if (font)
    {
      msdfgen::Shape shape;
      if (msdfgen::loadGlyph(shape, font, static_cast<msdfgen::unicode_t>(glyph)))
      {
        msdfgen::edgeColoringSimple(shape, 3);
        auto bounds = shape.getBounds();
        fmt::print("bounds: l: {} r: {} t: {} b: {}\n", bounds.l, bounds.r, bounds.t, bounds.b);
        const auto width = static_cast<int>(std::ceil(std::max(bounds.r, bounds.l)));
        const auto height = static_cast<int>(std::ceil(std::max(bounds.b, bounds.t)));
        msdfgen::Bitmap<float, 3> msdf(width, height);
        msdfgen::generateMSDF(msdf, shape, 4.0, 1.0, 0.0);
        auto tex = explot::make_texture();
        glBindTexture(GL_TEXTURE_2D, tex);
        const auto border_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(border_color));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT,
                     static_cast<const float *>(msdf));
        glGenerateMipmap(GL_TEXTURE_2D);
        return tex;
      }
    }
  }
  return explot::texture_handle();
}

auto make_tex_program()
{
  auto program = explot::make_program();
  auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_src, nullptr);
  glCompileShader(vertex_shader);
  auto fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
  glCompileShader(fragment_shader);
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // static constexpr auto varying = "uv";
  // glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  return program;
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

static constexpr explot::rect uv_space{{0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
} // namespace

namespace explot
{
fps_counter make_fps_counter()
{
  auto vbo = make_tex_vbo();
  auto vao = make_vao();
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  // auto debug_vbo = make_vbo();
  // glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, debug_vbo);
  // glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 2 * num_points * sizeof(float),
  //             nullptr, GL_DYNAMIC_DRAW);
  return fps_counter{
      .texture = make_font_texture("/usr/share/fonts/OTF/Monaco for Powerline.otf", 'A'),
      .vao = std::move(vao),
      .program = make_tex_program(),
      .vbo = std::move(vbo),
      //.debug_vbo = std::move(debug_vbo)
  };
}

void draw(const fps_counter &counter, const rect &r, const glm::mat4 &screen_to_clip)
{
  const auto uv_to_clip = screen_to_clip * transform(uv_space, r);
  // fmt::print("uv_to_clip: {}\n", glm::to_string(uv_to_clip));
  glBindTexture(GL_TEXTURE_2D, counter.texture);
  glBindVertexArray(counter.vao);
  glUseProgram(counter.program);
  glUniformMatrix4fv(glGetUniformLocation(counter.program, "uv_to_clip"), 1, GL_FALSE,
                     glm::value_ptr(uv_to_clip));
  // glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, counter.debug_vbo, 0,
  //                   2 * num_points * sizeof(float));
  // glBeginTransformFeedback(GL_TRIANGLES);
  glDrawArrays(GL_TRIANGLES, 0, num_points);
  // glEndTransformFeedback();
  //  auto b = glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, GL_READ_ONLY);
  //   float points[2 * num_points];
  //   std::memcpy(points, b, 2 * num_points * sizeof(float));
  //   glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
  //   fmt::print("({}, {}), ({}, {}), ({}, {}), ({}, {}), ({}, {}), ({},
  //   {})\n",
  //              points[0], points[1], points[2], points[3], points[4],
  //              points[5], points[6], points[7], points[8], points[9],
  //              points[10], points[11]);
}
} // namespace explot
