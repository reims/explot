#pragma once

#include <vector>
#include <span>
#include <variant>
#include "font_atlas.hpp"
#include "gl-handle.hpp"
#include "line_drawing.hpp"
#include "point_drawing.hpp"
#include "commands.hpp"
#include "line_type.hpp"

namespace explot
{
struct legend
{
  using mark_state = std::variant<points_2d_state, lines_state_2d>;

  font_atlas font;
  std::vector<gl_string> titles;
  std::vector<mark_state> marks;
  std::vector<glm::vec4> colors;
  std::vector<vbo_handle> vbos;

  legend(std::span<const graph_desc_2d> graphs);
  legend(std::span<const graph_desc_3d> graphs);
};

void update(const legend &l, const rect &screen, const glm::mat4 &screen_to_clip);
void draw(const legend &l);
} // namespace explot
