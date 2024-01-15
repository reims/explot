#pragma once
#include "rx.hpp"
#include "rect.hpp"
#include "commands.hpp"

namespace explot
{
rx::observable<rx::observable<unit>> plot_renderer(rx::observe_on_one_worker &on_run_loop,
                                                   rx::observable<unit> frames,
                                                   rx::observable<rect> screen_space,
                                                   rx::observable<plot_command_2d> plot_commands);

rx::observable<rx::observable<unit>> splot_renderer(rx::observe_on_one_worker &on_run_loop,
                                                    rx::observable<unit> frames,
                                                    rx::observable<rect> screen_space,
                                                    rx::observable<plot_command_3d> splot_commands);
} // namespace explot
