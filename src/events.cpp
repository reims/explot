#include "events.hpp"
#include <algorithm>
#include <fmt/format.h>

namespace
{
rx::subjects::subject<glm::vec2> mouse_move_subject;
auto mouse_move_subscriber = mouse_move_subject.get_subscriber();
auto mouse_move_observable = mouse_move_subject.get_observable() | rx::publish() | rx::ref_count();
rx::subjects::subject<int> mouse_down_subject;
auto mouse_down_subscriber = mouse_down_subject.get_subscriber();
auto mouse_down_observable = mouse_down_subject.get_observable() | rx::publish() | rx::ref_count();
rx::subjects::subject<int> mouse_up_subject;
auto mouse_up_subscriber = mouse_up_subject.get_subscriber();
auto mouse_up_observable = mouse_up_subject.get_observable() | rx::publish() | rx::ref_count();
auto drags_observable =
    mouse_down_observable
    | rx::with_latest_from(
        [](int, glm::vec2 from)
        {
          return mouse_move_observable
                 | rx::transform([from](glm::vec2 to)
                                 { return explot::drag{.from = from, .to = to}; })
                 | rx::take_until(mouse_up_observable) | rx::publish() | rx::ref_count()
                 | rx::as_dynamic();
        },
        mouse_move_observable)
    | rx::publish() | rx::ref_count();

auto drops_observable = drags_observable | rx::transform([](auto ds) { return ds | rx::last(); })
                        | rx::concat() | rx::publish() | rx::ref_count();
rx::subjects::subject<int> key_press_subject;
auto key_press_subscriber = key_press_subject.get_subscriber();
auto key_press_observable = key_press_subject.get_observable();

void mouse_move_cb(GLFWwindow *, double xpos, double ypos)
{
  mouse_move_subscriber.on_next(glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos)));
}

void mouse_button_cb(GLFWwindow *, int button, int action, int)
{
  if (action == GLFW_PRESS)
  {
    mouse_down_subscriber.on_next(button);
  }
  else if (action == GLFW_RELEASE)
  {
    mouse_up_subscriber.on_next(button);
  }
}

void key_press_cb(GLFWwindow *, int key, int, int action, int)
{
  if (action == GLFW_PRESS)
  {
    key_press_subscriber.on_next(key);
  }
}
} // namespace

namespace explot
{
void init_events(GLFWwindow *window)
{
  glfwSetCursorPosCallback(window, mouse_move_cb);
  glfwSetMouseButtonCallback(window, mouse_button_cb);
  glfwSetKeyCallback(window, key_press_cb);
}

rx::observable<glm::vec2> mouse_moves() { return mouse_move_observable; }
rx::observable<int> mouse_ups() { return mouse_up_observable; }
rx::observable<int> mouse_downs() { return mouse_down_observable; }
rx::observable<rx::observable<drag>> drags() { return drags_observable; }
rx::observable<drag> drops() { return drops_observable; }
rx::observable<int> key_presses() { return key_press_observable; }

rect drag_to_rect(const drag &d, float height)
{
  auto [min_x, max_x] = std::minmax(d.from.x, d.to.x);
  auto [min_y, max_y] = std::minmax(d.from.y, d.to.y);
  return {.lower_bounds = {min_x, height - max_y, -1.0f},
          .upper_bounds = {max_x, height - min_y, 1.0f}};
}

rect drag_to_rect(const drag &d)
{
  auto [min_x, max_x] = std::minmax(d.from.x, d.to.x);
  auto [min_y, max_y] = std::minmax(d.from.y, d.to.y);
  return {.lower_bounds = {min_x, min_y, -1.0f}, .upper_bounds = {max_x, max_y, 1.0f}};
}

rx::observable<rx::observable<drag>> drags(rx::observable<rect> screen, const rect &part)
{
  auto mousedowns = mouse_down_observable.with_latest_from([](int, glm::vec2 pos) { return pos; },
                                                           mouse_move_observable);
  auto all_drags =
      mousedowns
          .with_latest_from(
              [=](glm::vec2 pos, const rect &screen)
              {
                auto height = screen.upper_bounds.y - screen.lower_bounds.y;
                auto screen_pos = glm::vec2(pos.x, height - pos.y);
                return std::make_tuple(screen_pos, part_of(screen, part),
                                       mouse_move_observable.take_until(mouse_up_observable)
                                           .transform(
                                               [=](const glm::vec2 to)
                                               {
                                                 return drag{.from = screen_pos,
                                                             .to = glm::vec2(to.x, height - to.y)};
                                               })
                                           .as_dynamic());
              },
              screen)
          .filter([](const auto &t) { return contains(std::get<1>(t), std::get<0>(t)); })
          .transform([](const auto &t) { return std::get<2>(t); });
  return all_drags;
}
} // namespace explot
