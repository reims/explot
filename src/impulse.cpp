#include "impulse.hpp"
#include "data.hpp"

namespace explot
{
void draw(const impulses_state &s) { draw(s.lines); }

impulses_state::impulses_state(gl_id vbo, const seq_data_desc &d, float width,
                               const glm::vec4 &color)
    : lines(vbo, reshape(d, 2), width, color)
{
}

} // namespace explot
