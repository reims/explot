#include "rect.hpp"
#include <GL/glew.h>
#include "rx.hpp"
#include <GLFW/glfw3.h>
#include <vector>
#include <linenoise.h>
#include <string_view>
#include <fmt/format.h>
#include "parse_commands.hpp"
#include "events.hpp"
#include "colors.hpp"
#include "settings.hpp"
#include <thread>
#include "rx-renderers.hpp"

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
  glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET);

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
  auto frames = frames_subject.get_observable() | rx::publish() | rx::ref_count();
  auto frames_out = frames_subject.get_subscriber();
  rxcpp::schedulers::run_loop rl;
  auto on_run_loop = rx::observe_on_run_loop(rl);

  rxcpp::subjects::behavior<rect> screen_space_subject(rect{});
  auto screen_space = screen_space_subject.get_observable() | rx::distinct_until_changed();
  //                      | rx::publish() | rx::ref_count();
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

  auto plot = plot_renderer(on_run_loop, frames, screen_space, plot_commands);

  auto splot_commands =
      commands
      | rx::filter([](const command &cmd) { return std::holds_alternative<plot_command_3d>(cmd); })
      | rx::transform([](const command &cmd) { return std::get<plot_command_3d>(cmd); });

  auto splot = splot_renderer(on_run_loop, frames, screen_space, splot_commands);

  auto plot_renderers = splot.as_dynamic().merge(plot) | rx::switch_on_next();
  renderers.push_back(plot_renderers);

  rx::composite_subscription lifetime;
  rx::iterate(renderers) | rx::merge() | rx::subscribe<unit>(lifetime, [](unit) {});

  commands | rx::filter(is_quit_command) | to_unit() | rx::observe_on(on_run_loop)
      | rx::subscribe<unit>(lifetime, [=](unit) { glfwSetWindowShouldClose(window, GL_TRUE); });

  while (!glfwWindowShouldClose(window))
  {
    // fmt::print("start frame\n");
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
    glFlush();
    // fmt::print("end frame\n");
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
  linenoiseHistoryLoad("build/src/history");

  auto uithread =
      std::jthread(uimain, cmd_subject.get_observable() | rx::publish() | rx::ref_count());

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

  linenoiseHistorySave("build/src/history");

  return 0;
}
