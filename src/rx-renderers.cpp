#include "rx-renderers.hpp"
#include "drag_renderer.hpp"
#include "plot2d.hpp"
#include "plot3d.hpp"
#include "events.hpp"
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "coordinate_system_2d.hpp"
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
               auto drops = ds.transform([](auto d) { return d.default_if_empty(drag{}).last(); })
                                .concat()
                                .filter([](const drag &d) { return d.from != d.to; });
               auto local_screen = screen_space.transform(
                   [=](const rect &screen)
                   {
                     auto local = part_of(screen, part);
                     // fmt::println("rel screen: [{}, {}]x[{}, {}], local: [{}, {}]x[{}, {}],
                     // whole: "
                     //              "[{}, {}]x[{}, {}]",
                     //              part.lower_bounds.x, part.upper_bounds.x, part.lower_bounds.y,
                     //              part.upper_bounds.y, local.lower_bounds.x,
                     //              local.upper_bounds.x, local.lower_bounds.y,
                     //              local.upper_bounds.y, screen.lower_bounds.x,
                     //              screen.upper_bounds.x, screen.lower_bounds.y,
                     //              screen.upper_bounds.y);
                     return local;
                   });
               auto view_space_obs =
                   const_get(res).view_space.get_observable() | rx::publish() | rx::ref_count();
               auto selections = drops
                                 | rx::with_latest_from(
                                     [](const drag &r, const rect &screen, const rect &view)
                                     {
                                       auto screen_to_view = transform(screen, view);
                                       return transform(drag_to_rect(r), screen_to_view);
                                     },
                                     local_screen, view_space_obs)
                                 | rx::publish() | rx::ref_count();
               auto resets = key_presses() | rx::filter([](int key) { return key == GLFW_KEY_A; })
                             | rx::transform([phase_space = round_for_ticks_2d(
                                                  const_get(res).plot.phase_space, 5, 2)](int)
                                             { return phase_space; });
               auto phase_space =
                   selections
                   | rx::transform([](const rect &r) { return round_for_ticks_2d(r, 5, 2); })
                   | rx::merge(resets)
                   | rx::start_with(round_for_ticks_2d(const_get(res).plot.phase_space, 5, 2));
               auto view_space = phase_space
                                 | rx::transform([](const tics_desc &r)
                                                 { return scale2d(r.bounding_rect, 1.1f); });
               auto sub = view_space.subscribe(const_get(res).view_space.get_subscriber());
               auto coordinate_systems =
                   phase_space | rx::observe_on(on_run_loop)
                   | rx::transform(
                       [timebase = const_get(res).plot.timebase](const tics_desc &r)
                       {
                         return std::make_shared<coordinate_system_2d>(r, 5, 9.0f, 2.0f, timebase);
                       })
                   | rx::publish() | rx::ref_count();
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
                                                   glViewport(
                                                       static_cast<GLint>(r.lower_bounds.x),
                                                       static_cast<GLint>(r.lower_bounds.y),
                                                       static_cast<GLsizei>(r.upper_bounds.x
                                                                            - r.lower_bounds.x),
                                                       static_cast<GLsizei>(r.upper_bounds.y
                                                                            - r.lower_bounds.y));
                                                   draw(const_get(res), drag_to_rect(d), r);
                                                   return unit{};
                                                 },
                                                 ds, local_screen);
                                    })
                                | rx::subscribe_on(on_run_loop);
                       })
                   | rx::switch_on_next();

               auto updates = view_space.combine_latest(local_screen)
                                  .observe_on(on_run_loop)
                                  .with_latest_from(
                                      [res](const std::tuple<rect, rect> &views,
                                            const std::shared_ptr<coordinate_system_2d> &cs)
                                      {
                                        auto &[view, screen] = views;
                                        update(const_get(res).plot, screen, view);
                                        auto transforms = transforms_2d{
                                            .phase_to_screen = transform(view, screen),
                                            .screen_to_clip = transform(screen, clip_rect)};
                                        update(*cs, transforms);
                                        return unit{};
                                      },
                                      coordinate_systems);

               return frames | rx::observe_on(on_run_loop)
                      | rx::with_latest_from(
                          [res](unit, unit, const std::shared_ptr<coordinate_system_2d> &cs,
                                const rect &r)
                          {
                            glViewport(static_cast<GLint>(r.lower_bounds.x),
                                       static_cast<GLint>(r.lower_bounds.y),
                                       static_cast<GLsizei>(r.upper_bounds.x - r.lower_bounds.x),
                                       static_cast<GLsizei>(r.upper_bounds.y - r.lower_bounds.y));
                            draw(const_get(res).plot);
                            draw(*cs);
                            return unit{};
                          },
                          updates, coordinate_systems, local_screen)
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
                                      [res](const std::tuple<glm::mat4, rect> &views)
                                      {
                                        auto &[rot, screen] = views;
                                        auto dir =
                                            glm::transpose(rot) * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                                        update(const_get(res), -4.0f * glm::vec3(dir), rot, screen);
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
