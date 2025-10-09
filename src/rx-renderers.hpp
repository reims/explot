#pragma once
#include "rx.hpp"
#include "rect.hpp"
#include "commands.hpp"

namespace explot
{

rx::observable<unit> plot_renderer(rx::observe_on_one_worker &on_run_loop,
                                   rx::observable<unit> frames, rx::observable<rect> screen_space,
                                   rect part, const plot_command_2d &cmd);

rx::observable<unit> splot_renderer(rx::observe_on_one_worker &on_run_loop,
                                    rx::observable<unit> frames, rx::observable<rect> screen_space,
                                    rect part, const plot_command_3d &cmd);

} // namespace explot
