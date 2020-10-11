#include "fps_counter.hpp"
#include "rect.hpp"
// #include <memory>
#define GLEW_STATIC
// #include <GL/glew.h>
#include "plot2d.hpp"
#include "plot3d.hpp"
#include "graph2d.hpp"
#include "rx.hpp"
#include "data.hpp"
#include <GLFW/glfw3.h>
// #include <array>
#include <vector>
#include <linenoise.h>
#include <string_view>
#include "font_atlas.hpp"
#include <fmt/format.h>
#include <glm/gtx/string_cast.hpp>
#include "parse_commands.hpp"
#include "csv.hpp"
#include "events.hpp"
#include "drag_renderer.hpp"
#include "colors.hpp"
#include "settings.hpp"
#include <thread>

namespace
{
using namespace std::literals::chrono_literals;

void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                      const GLchar *message, const void *userParam)
{
  if (type == GL_DEBUG_TYPE_ERROR)
  {
    fmt::print("Error: {} '{}'\n", id, message);
  }
}

struct plot_with_view_space
{
  explot::plot2d plot;
  rx::subjects::behavior<explot::rect> view_space;

  explicit plot_with_view_space(explot::plot2d plot)
      : plot(std::move(plot)),
        view_space(explot::scale2d(explot::round_for_ticks_2d(plot.phase_space, 5, 2), 1.1))
  {
  }
};

// workaround for missing const overload of resource<T>::get
// this should not cause UB, since the returned reference is const
template <typename T>
const T &const_get(const rx::resource<T> &res)
{
  return const_cast<rx::resource<T> &>(res).get();
}

static constexpr auto uimain = [](auto commands)
{
  using namespace explot;

  if (!glfwInit())
  {
    return;
  }

  glfwSetErrorCallback(
      [](int error, const char *description)
      {
        fmt::print("error {} : {}\n", error, description);
        // std::exit(-1);
      });
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  auto *window = glfwCreateWindow(640, 480, "explot", nullptr, nullptr);
  if (window == nullptr)
  {
    fmt::print("Failed to create window\n");
    return;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glewInit();

  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(message_callback, 0);

  init_events(window);

  rxcpp::subjects::subject<unit> frames_subject;
  auto frames = frames_subject.get_observable();
  auto frames_out = frames_subject.get_subscriber();
  rxcpp::schedulers::run_loop rl;
  auto on_run_loop = rx::observe_on_run_loop(rl);

  rxcpp::subjects::subject<rect> screen_space_subject;
  auto screen_space = screen_space_subject.get_observable();
  auto screen_space_out = screen_space_subject.get_subscriber();

  auto renderers = std::vector<rx::observable<unit>>{};
  auto show_renderer = commands
                       | rx::transform(
                           [](const command &cmd)
                           {
                             if (std::holds_alternative<show_command>(cmd))
                             {
                               fmt::print("{}\n", settings::show(std::get<show_command>(cmd).path));
                             }
                             return unit{};
                           });
  renderers.push_back(show_renderer);
  auto set_renderer = commands
                      | rx::transform(
                          [](const command &cmd)
                          {
                            if (std::holds_alternative<set_command>(cmd))
                            {
                              const auto &set_cmd = std::get<set_command>(cmd);
                              if (settings::set(set_cmd.path, set_cmd.value))
                              {
                                fmt::print("set value\n");
                              }
                              else
                              {
                                fmt::print("set failed\n");
                              }
                            }
                            return unit{};
                          });
  renderers.push_back(set_renderer);

  auto plot_commands = commands | rx::filter(is_plot_command)
                       | rx::transform([](const command &cmd) { return as_plot_command(cmd); });

  auto plot_renderer =
      plot_commands | rx::observe_on(on_run_loop)
      | rx::transform(
          [=](plot_command_2d cmd)
          {
            return rx::scope(
                       [cmd = std::move(cmd)]() {
                         return rx::resource<plot_with_view_space>(
                             plot_with_view_space(make_plot2d(cmd)));
                       },
                       [=](rx::resource<plot_with_view_space> res)
                       {
                         auto view_space_obs = const_get(res).view_space.get_observable()
                                               | rx::publish() | rx::ref_count();
                         auto selections =
                             drops()
                             | rx::with_latest_from(
                                 [](const drag &r, const rect &screen, const rect &view)
                                 {
                                   auto screen_to_view = transform(screen, view);
                                   return transform(drag_to_rect(r, screen.upper_bounds.y),
                                                    screen_to_view);
                                 },
                                 screen_space, view_space_obs)
                             | rx::publish() | rx::ref_count();
                         auto resets =
                             key_presses() | rx::filter([](int key) { return key == GLFW_KEY_A; })
                             | rx::transform([phase_space = round_for_ticks_2d(
                                                  const_get(res).plot.phase_space, 5, 2)](int)
                                             { return phase_space; });
                         auto phase_space = selections
                                            | rx::transform([](const rect &r)
                                                            { return round_for_ticks_2d(r, 5, 2); })
                                            | rx::merge(resets)
                                            | rx::start_with(round_for_ticks_2d(
                                                const_get(res).plot.phase_space, 5, 2));
                         auto view_space =
                             phase_space
                             | rx::transform([](const rect &r) { return scale2d(r, 1.1f); });
                         auto sub =
                             view_space.subscribe(const_get(res).view_space.get_subscriber());
                         auto coordinate_systems =
                             phase_space | rx::observe_on(on_run_loop)
                             | rx::transform(
                                 [](const rect &r) {
                                   return std::make_shared<coordinate_system_2d>(
                                       make_coordinate_system_2d(r, 5));
                                 });
                         return frames | rx::observe_on(on_run_loop)
                                | rx::with_latest_from(
                                    [res](unit, const rect &screen, const rect &view,
                                          const std::shared_ptr<coordinate_system_2d> &cs)
                                    {
                                      auto view_to_screen = transform(view, screen);
                                      auto screen_to_clip = transform(screen, clip_rect);
                                      draw(const_get(res).plot, view_to_screen, screen_to_clip);
                                      draw(*cs, view_to_screen, screen_to_clip, 1.0f, 5.0f);
                                      return unit{};
                                    },
                                    screen_space, view_space, coordinate_systems);
                       })
                   | rx::subscribe_on(on_run_loop) | rx::as_dynamic();
          });

  auto splot_commands =
      commands
      | rx::filter([](const command &cmd) { return std::holds_alternative<plot_command_3d>(cmd); })
      | rx::transform([](const command &cmd) { return std::get<plot_command_3d>(cmd); });

  auto splot_renderer =
      splot_commands | rx::observe_on(on_run_loop)
      | rx::transform(
          [=](plot_command_3d cmd)
          {
            auto rel_rots = drags()
                            | rx::transform(
                                [](const rx::observable<drag> &ds)
                                {
                                  return ds
                                         | rx::zip(
                                             [](const drag &d1, const drag &d2)
                                             {
                                               auto v = d2.to - d1.to;
                                               v = glm::vec2(v.y, v.x);
                                               if (v.x == 0.0 && v.y == 0)
                                               {
                                                 return glm::identity<glm::mat4>();
                                               }

                                               return glm::rotate(
                                                   glm::identity<glm::mat4>(), 0.01f * v.length(),
                                                   glm::vec3(glm::normalize(v), 0.0f));
                                             },
                                             ds | rx::skip(1));
                                })
                            | rx::switch_on_next();

            auto rots = rel_rots
                        | rx::scan(glm::identity<glm::mat4>(),
                                   [](const glm::mat4 &rot, const glm::mat4 &rel_rot)
                                   { return rel_rot * rot; })
                        | rx::start_with(glm::identity<glm::mat4>());
            return rx::scope([cmd = std::move(cmd)]() { return rx::resource(plot3d(cmd)); },
                             [=](rx::resource<plot3d> res)
                             {
                               return frames | rx::observe_on(on_run_loop)
                                      | rx::with_latest_from(
                                          [res = std::move(res)](unit, const rect &screen,
                                                                 const glm::mat4 &rot)
                                          {
                                            auto dir = glm::transpose(rot)
                                                       * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                                            draw(const_get(res), -2.0f * glm::vec3(dir), rot,
                                                 screen);
                                            return unit{};
                                          },
                                          screen_space, rots);
                             })
                   | rx::subscribe_on(on_run_loop) | rx::as_dynamic();
          });

  auto plot_renderers = plot_renderer.as_dynamic().merge(splot_renderer) | rx::switch_on_next();
  renderers.push_back(plot_renderers);

  auto drag_renderer =
      drags()
      | rx::transform(
          [=](auto ds)
          {
            return rx::scope(
                       []() { return rx::resource<drag_render_state>(make_drag_render_state()); },
                       [=, ds = ds | rx::publish()
                                | rx::ref_count()](rx::resource<drag_render_state> res)
                       {
                         return frames | rx::take_until(ds | rx::last())
                                | rx::observe_on(on_run_loop)
                                | rx::with_latest_from(
                                    [res = std::move(res)](unit, const rect &screen, const drag &d)
                                    {
                                      auto screen_to_clip = transform(screen, clip_rect);
                                      draw(const_get(res), drag_to_rect(d, screen.upper_bounds.y),
                                           screen_to_clip, 1.0f);
                                      return unit{};
                                    },
                                    screen_space, ds);
                       })
                   | rx::subscribe_on(on_run_loop);
          })
      | rx::switch_on_next();
  renderers.push_back(drag_renderer);

  auto fps_renderer = rx::scope(
      []()
      {
        auto atlas = make_font_atlas("0123456789fps").value();
        return rx::resource<font_atlas>(std::move(atlas));
      },
      [=](rx::resource<font_atlas> res)
      {
        auto fps = frames | rx::sample_with_time(1s)
                   | rx::transform(
                       [frames](unit)
                       {
                         return frames | rx::take_until(std::chrono::steady_clock::now() + 1s)
                                | rxcpp::operators::count(); // rx::count is hidden
                       })
                   | rx::switch_on_next();
        return fps | rx::observe_on(on_run_loop)
               | rx::transform(
                   [=, res = std::move(res)](int n)
                   {
                     return rx::scope(
                                [res = std::move(res), n]() {
                                  return rx::resource<gl_string>(
                                      make_gl_string(const_get(res), fmt::format("{}fps", n)));
                                },
                                [=](rx::resource<gl_string> str)
                                {
                                  return frames | rx::observe_on(on_run_loop)
                                         | rx::with_latest_from(
                                             [str = std::move(str)](unit, const rect &screen)
                                             {
                                               const auto screen_to_clip =
                                                   transform(screen, clip_rect);
                                               draw(const_get(str), screen_to_clip,
                                                    {screen.upper_bounds.x - 10.0f,
                                                     screen.upper_bounds.y - 10.0f},
                                                    text_color, 1.0f, {1.0f, 1.0f});
                                               return unit{};
                                             },
                                             screen_space);
                                })
                            | rx::subscribe_on(on_run_loop);
                   })
               | rx::switch_on_next();
      });
  renderers.push_back(fps_renderer);

  rx::composite_subscription lifetime;
  rx::iterate(renderers) | rx::merge() | rx::subscribe<unit>(lifetime, [](unit) {});

  commands | rx::filter(is_quit_command) | to_unit() | rx::observe_on(on_run_loop)
      | rx::subscribe<unit>(lifetime, [=](unit) { glfwSetWindowShouldClose(window, GL_TRUE); });

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    screen_space_out.on_next(rect{.lower_bounds = {0, 0, -1}, .upper_bounds = {width, height, 1}});

    frames_out.on_next(unit{});

    while (!rl.empty() && rl.peek().when < rl.now())
    {
      rl.dispatch();
    }

    glfwSwapBuffers(window);
  }

  lifetime.unsubscribe();

  glfwDestroyWindow(window);
  glfwTerminate();
};

} // namespace

int main()
{
  auto cmd_subject = rx::subjects::subject<explot::command>();
  auto cmd_subscriber = cmd_subject.get_subscriber();

  linenoiseHistorySetMaxLen(100);
  linenoiseHistoryLoad("build/history");

  auto uithread = std::jthread(uimain, cmd_subject.get_observable());

  for (auto line_ptr = std::unique_ptr<char>(linenoise("> ")); line_ptr != nullptr;
       line_ptr.reset(linenoise("> ")))
  {
    linenoiseHistoryAdd(line_ptr.get());
    auto cmd = explot::parse_command(line_ptr.get());
    if (cmd.has_value())
    {
      cmd_subscriber.on_next(cmd.value());
      if (std::holds_alternative<explot::quit_command>(cmd.value()))
      {
        break;
      }
    }
    else
    {
      fmt::print("unknown command\n");
    }
  }

  linenoiseHistorySave("build/history");

  return 0;
}
