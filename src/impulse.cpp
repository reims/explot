#include "impulse.hpp"

namespace explot
{
void draw(const impulses_state &s) { draw(s.lines); }

impulses_state::impulses_state(data_desc d, float width, const glm::vec4 &color)
    : lines(reshape(std::move(d), 2), width, color)
{
}

} // namespace explot
