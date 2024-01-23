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
#include <atomic>
#include <semaphore>
#include <cstdlib>
#include <filesystem>

namespace
{
using namespace std::literals::chrono_literals;
using namespace explot;
void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                      const GLchar *message, const void *userParam)
{
  if (type == GL_DEBUG_TYPE_ERROR)
  {
    fmt::print("Error: {} '{}'\n", id, message);
  }
}

static std::atomic<bool> thread_running = false;
static std::binary_semaphore ready_for_cmds(0);

static constexpr auto uimain = [](auto commands)
{
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

  auto screen_space_out = screen_space_subject.get_subscriber();

  auto renderers = std::vector<rx::observable<unit>>{};
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

  auto q_presses =
      key_presses() | rx::filter([](int key) { return key == GLFW_KEY_Q; }) | to_unit();
  commands | rx::filter(is_quit_command) | to_unit() | rx::merge(q_presses)
      | rx::observe_on(on_run_loop)
      | rx::subscribe<unit>(lifetime, [=](unit) { glfwSetWindowShouldClose(window, GL_TRUE); });

  ready_for_cmds.release();

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

  thread_running.store(false);
};

std::filesystem::path get_data_dir()
{
  const auto data_dir_ptr = std::getenv("XDG_DATA_HOME");
  if (data_dir_ptr == nullptr)
  {
    const auto home = std::getenv("HOME");
    if (home == nullptr)
    {
      fmt::println("$HOME and $XDG_DATA_HOME not set");
      return std::filesystem::current_path();
    }
    return std::filesystem::path(home) / ".local" / "share" / "explot";
  }
  return std::filesystem::path(data_dir_ptr) / "explot";
}

} // namespace

int main()
{
  auto cmd_subject = rx::subjects::subject<explot::command>();
  auto cmd_subscriber = cmd_subject.get_subscriber();

  const auto data_dir = get_data_dir();

  std::filesystem::create_directories(data_dir);
  const auto history_path = data_dir / "history";

  linenoiseHistorySetMaxLen(100);
  linenoiseHistoryLoad(history_path.c_str());

  auto uithread = std::optional<std::jthread>();

  for (auto line_ptr = std::unique_ptr<char>(linenoise("> ")); line_ptr != nullptr;
       line_ptr.reset(linenoise("> ")))
  {
    linenoiseHistoryAdd(line_ptr.get());
    auto cmd = explot::parse_command(line_ptr.get());
    if (cmd.has_value())
    {
      if (std::holds_alternative<explot::quit_command>(cmd.value()))
      {
        cmd_subscriber.on_next(cmd.value());
        break;
      }
      else if (std::holds_alternative<show_command>(cmd.value()))
      {
        fmt::print("{}\n", settings::show(std::get<show_command>(cmd.value()).path));
      }
      else if (std::holds_alternative<set_command>(cmd.value()))
      {
        const auto &set_cmd = std::get<set_command>(cmd.value());
        if (settings::set(set_cmd.path, set_cmd.value))
        {
          fmt::print("set value\n");
        }
        else
        {
          fmt::print("set failed\n");
        }
      }
      else
      {
        if (!thread_running.load())
        {
          uithread = std::jthread(uimain, cmd_subject.get_observable());
          thread_running.store(true);
          ready_for_cmds.acquire();
        }
        cmd_subscriber.on_next(cmd.value());
      }
    }
    else
    {
      fmt::print("unknown command\n");
    }
  }

  linenoiseHistorySave(history_path.c_str());

  return 0;
}
