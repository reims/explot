#include "rx-renderers.hpp"
#include "drag_renderer.hpp"
#include "plot2d.hpp"
#include "plot3d.hpp"
#include "events.hpp"
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fmt/format.h>

namespace
{
using namespace explot;
struct plot_with_view_space
{
  plot2d plot;
  rx::subjects::behavior<rect> view_space;

  explicit plot_with_view_space(plot2d plot)
      : plot(std::move(plot)),
        view_space(scale2d(round_for_ticks_2d(plot.phase_space, 5, 2).bounding_rect, 1.1f))
  {
  }
};

template <typename T>
const T &const_get(const rx::resource<T> &res)
{
  return const_cast<rx::resource<T> &>(res).get();
}

void set_viewport(const rect &r)
{
  glViewport(static_cast<GLint>(r.lower_bounds.x), static_cast<GLint>(r.lower_bounds.y),
             static_cast<GLsizei>(r.upper_bounds.x - r.lower_bounds.x),
             static_cast<GLsizei>(r.upper_bounds.y - r.lower_bounds.y));
}

} // namespace

namespace explot
{
rx::observable<unit> plot_renderer(rx::observe_on_one_worker &on_run_loop,
                                   rx::observable<unit> frames, rx::observable<rect> screen_space,
                                   rect part, const plot_command_2d &cmd)
{
  return rx::scope(
             [cmd = std::move(cmd)]()
             { return rx::resource<plot_with_view_space>(plot_with_view_space(plot2d(cmd))); },
             [=](rx::resource<plot_with_view_space> res)
             {
               auto ds = drags(screen_space, part).publish().ref_count();
               auto drops =
                   ds | rx::transform([](auto d) { return d.default_if_empty(drag{}).last(); })
                   | rx::concat() | rx::filter([](const drag &d) { return d.from != d.to; });
               auto local_screen = screen_space
                                   | rx::transform(
                                       [=](const rect &screen)
                                       {
                                         auto local = part_of(screen, part);
                                         return local;
                                       });
               static constexpr auto lower_margin = glm::vec3(100.0f, 50.0f, 0.0f);
               static constexpr auto upper_margin = glm::vec3(50.0f, 20.0f, 0.0f);
               auto plot_screen =
                   local_screen
                   | rx::transform([](const rect &screen)
                                   { return remove_margin(screen, lower_margin, upper_margin); });

               auto view_space_obs = const_get(res).view_space.get_observable();
               auto selections = drops
                                 | rx::with_latest_from(
                                     [](const drag &r, const rect &screen, const rect &view)
                                     {
                                       auto screen_to_view = transform(screen, view);
                                       return transform(drag_to_rect(r), screen_to_view);
                                     },
                                     plot_screen, view_space_obs);
               auto resets = key_presses() | rx::filter([](int key) { return key == GLFW_KEY_A; })
                             | rx::transform([phase_space = const_get(res).plot.phase_space](int)
                                             { return phase_space; });
               auto phase_space =
                   selections | rx::merge(resets) | rx::start_with(const_get(res).plot.phase_space);

               auto sub = phase_space.subscribe(const_get(res).view_space.get_subscriber());
               auto drag_renderer =
                   ds
                   | rx::transform(
                       [=](auto ds)
                       {
                         return rx::scope(
                                    [=]()
                                    {
                                      return rx::resource<drag_render_state>(drag_render_state());
                                    },
                                    [=](rx::resource<drag_render_state> res)
                                    {
                                      return frames | rx::observe_on(on_run_loop)
                                             | rx::take_until(closed(ds))
                                             | rx::with_latest_from(
                                                 [res = std::move(res)](unit, const drag &d,
                                                                        const rect &r)
                                                 {
                                                   set_viewport(r);
                                                   draw(const_get(res), drag_to_rect(d), r);
                                                   return unit{};
                                                 },
                                                 ds, local_screen);
                                    })
                                | rx::subscribe_on(on_run_loop);
                       })
                   | rx::switch_on_next();

               auto view_updates = phase_space.observe_on(on_run_loop)
                                       .transform(
                                           [res](const rect &view) mutable
                                           {
                                             update_view(res.get().plot, view);
                                             return unit{};
                                           });

               auto screen_updates = local_screen.transform(
                   [res](const rect &screen) mutable
                   {
                     update_screen(res.get().plot, screen);
                     return unit{};
                   });

               auto updates = view_updates | rx::merge(screen_updates);
               return frames | rx::observe_on(on_run_loop)
                      | rx::with_latest_from(
                          [res](unit, unit)
                          {
                            draw(const_get(res).plot);
                            return unit{};
                          },
                          updates)
                      | rx::merge(drag_renderer);
             })
         | rx::subscribe_on(on_run_loop);
}

rx::observable<unit> splot_renderer(rx::observe_on_one_worker &on_run_loop,
                                    rx::observable<unit> frames, rx::observable<rect> screen_space,
                                    rect part, const plot_command_3d &cmd)
{
  auto ds = drags(screen_space, part).publish().ref_count();
  auto local_screen =
      screen_space.transform([=](const rect &screen) { return part_of(screen, part); });

  auto rel_rots =
      ds
      | rx::transform(
          [](const rx::observable<drag> &ds)
          {
            return ds
                   | rx::zip(
                       [](const drag &d1, const drag &d2)
                       {
                         auto v = d2.to - d1.to;
                         v = glm::vec2(-v.y, v.x);
                         if (v.x == 0.0 && v.y == 0)
                         {
                           return glm::identity<glm::mat4>();
                         }

                         return glm::rotate(glm::identity<glm::mat4>(), 0.01f * v.length(),
                                            glm::vec3(glm::normalize(v), 0.0f));
                       },
                       ds | rx::skip(1));
          })
      | rx::switch_on_next();

  auto rots =
      rel_rots
      | rx::scan(glm::identity<glm::mat4>(),
                 [](const glm::mat4 &rot, const glm::mat4 &rel_rot) { return rel_rot * rot; })
      | rx::start_with(glm::identity<glm::mat4>());
  return rx::scope(
             [cmd = std::move(cmd)]() { return rx::resource(plot3d(cmd)); },
             [=](rx::resource<plot3d> res)
             {
               auto updates = rots.combine_latest(local_screen)
                                  .observe_on(on_run_loop)
                                  .transform(
                                      [res](const std::tuple<glm::mat4, rect> &views) mutable
                                      {
                                        auto &[rot, screen] = views;
                                        auto dir =
                                            glm::transpose(rot) * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                                        update(res.get(), -4.0f * glm::vec3(dir), rot, screen);
                                        return unit{};
                                      });
               return frames | rx::observe_on(on_run_loop)
                      | rx::with_latest_from(
                          [res](unit, unit, const rect &r)
                          {
                            glViewport(static_cast<GLint>(r.lower_bounds.x),
                                       static_cast<GLint>(r.lower_bounds.y),
                                       static_cast<GLsizei>(r.upper_bounds.x - r.lower_bounds.x),
                                       static_cast<GLsizei>(r.upper_bounds.y - r.lower_bounds.y));
                            draw(const_get(res));
                            return unit{};
                          },
                          updates, local_screen);
             })
         | rx::subscribe_on(on_run_loop) | rx::as_dynamic();
}
} // namespace explot
