#pragma once

#include "gl-handle.hpp"
#include "data.hpp"
#include <memory.h>
#include <glm/mat4x4.hpp>

namespace explot
{
struct surface_state final
{
  surface_state(std::shared_ptr<data_desc> data);

  std::shared_ptr<data_desc> data;
  vbo_handle indices;
  vao_handle vao;
  program_handle program;
  size_t num_points;
};

void draw(const surface_state &surface, const glm::mat4 &phase_to_view,
          const glm::mat4 &view_to_clip);
} // namespace explot
