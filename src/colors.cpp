#include "colors.hpp"
#include <utility>

namespace
{
using namespace std::string_view_literals;
using namespace explot;
static constexpr std::pair<std::string_view, glm::vec4> named_colors[] = {
    {"white"sv, from_rgb(0xffffff)},         {"black"sv, from_rgb(0x000000)},
    {"dark"sv, from_rgb(0xa0a0a0)},          {"red"sv, from_rgb(0xff0000)},
    {"web"sv, from_rgb(0x00c000)},           {"web"sv, from_rgb(0x0080ff)},
    {"dark"sv, from_rgb(0xc000ff)},          {"dark"sv, from_rgb(0x00eeee)},
    {"dark"sv, from_rgb(0xc04000)},          {"dark"sv, from_rgb(0xc8c800)},
    {"royalblue"sv, from_rgb(0x4169e1)},     {"goldenrod"sv, from_rgb(0xffc020)},
    {"dark"sv, from_rgb(0x008040)},          {"purple"sv, from_rgb(0xc080ff)},
    {"steelblue"sv, from_rgb(0x306080)},     {"dark"sv, from_rgb(0x8b0000)},
    {"dark"sv, from_rgb(0x408000)},          {"orchid"sv, from_rgb(0xff80ff)},
    {"aquamarine"sv, from_rgb(0x7fffd4)},    {"brown"sv, from_rgb(0xa52a2a)},
    {"yellow"sv, from_rgb(0xffff00)},        {"turquoise"sv, from_rgb(0x40e0d0)},
    {"grey0"sv, from_rgb(0x000000)},         {"grey10"sv, from_rgb(0x1a1a1a)},
    {"grey20"sv, from_rgb(0x333333)},        {"grey30"sv, from_rgb(0x4d4d4d)},
    {"grey40"sv, from_rgb(0x666666)},        {"grey50"sv, from_rgb(0x7f7f7f)},
    {"grey60"sv, from_rgb(0x999999)},        {"grey70"sv, from_rgb(0xb3b3b3)},
    {"grey"sv, from_rgb(0xc0c0c0)},          {"grey80"sv, from_rgb(0xcccccc)},
    {"grey90"sv, from_rgb(0xe5e5e5)},        {"grey100"sv, from_rgb(0xffffff)},
    {"light"sv, from_rgb(0xf03232)},         {"light"sv, from_rgb(0x90ee90)},
    {"light"sv, from_rgb(0xadd8e6)},         {"light"sv, from_rgb(0xf055f0)},
    {"light"sv, from_rgb(0xe0ffff)},         {"light"sv, from_rgb(0xeedd82)},
    {"light"sv, from_rgb(0xffb6c1)},         {"light"sv, from_rgb(0xafeeee)},
    {"gold"sv, from_rgb(0xffd700)},          {"green"sv, from_rgb(0x00ff00)},
    {"dark"sv, from_rgb(0x006400)},          {"spring"sv, from_rgb(0x00ff7f)},
    {"forest"sv, from_rgb(0x228b22)},        {"sea"sv, from_rgb(0x2e8b57)},
    {"blue"sv, from_rgb(0x0000ff)},          {"dark"sv, from_rgb(0x00008b)},
    {"midnight"sv, from_rgb(0x191970)},      {"navy"sv, from_rgb(0x000080)},
    {"medium"sv, from_rgb(0x0000cd)},        {"skyblue"sv, from_rgb(0x87ceeb)},
    {"cyan"sv, from_rgb(0x00ffff)},          {"magenta"sv, from_rgb(0xff00ff)},
    {"dark"sv, from_rgb(0x00ced1)},          {"dark"sv, from_rgb(0xff1493)},
    {"coral"sv, from_rgb(0xff7f50)},         {"light"sv, from_rgb(0xf08080)},
    {"orange"sv, from_rgb(0xff4500)},        {"salmon"sv, from_rgb(0xfa8072)},
    {"dark"sv, from_rgb(0xe9967a)},          {"khaki"sv, from_rgb(0xf0e68c)},
    {"dark"sv, from_rgb(0xbdb76b)},          {"dark"sv, from_rgb(0xb8860b)},
    {"beige"sv, from_rgb(0xf5f5dc)},         {"olive"sv, from_rgb(0xa08020)},
    {"orange"sv, from_rgb(0xffa500)},        {"violet"sv, from_rgb(0xee82ee)},
    {"dark"sv, from_rgb(0x9400d3)},          {"plum"sv, from_rgb(0xdda0dd)},
    {"dark"sv, from_rgb(0x905040)},          {"dark"sv, from_rgb(0x556b2f)},
    {"orangered4"sv, from_rgb(0x801400)},    {"brown4"sv, from_rgb(0x801414)},
    {"sienna4"sv, from_rgb(0x804014)},       {"orchid4"sv, from_rgb(0x804080)},
    {"mediumpurple3"sv, from_rgb(0x8060c0)}, {"slateblue1"sv, from_rgb(0x8060ff)},
    {"yellow4"sv, from_rgb(0x808000)},       {"sienna1"sv, from_rgb(0xff8040)},
    {"tan1"sv, from_rgb(0xffa040)},          {"sandybrown"sv, from_rgb(0xffa060)},
    {"light"sv, from_rgb(0xffa070)},         {"pink"sv, from_rgb(0xffc0c0)},
    {"khaki1"sv, from_rgb(0xffff80)},        {"lemonchiffon"sv, from_rgb(0xffffc0)},
    {"bisque"sv, from_rgb(0xcdb79e)},        {"honeydew"sv, from_rgb(0xf0fff0)},
    {"slategrey"sv, from_rgb(0xa0b6cd)},     {"seagreen"sv, from_rgb(0xc1ffc1)},
    {"antiquewhite"sv, from_rgb(0xcdc0b0)},  {"chartreuse"sv, from_rgb(0x7cff40)},
    {"greenyellow"sv, from_rgb(0xa0ff20)},   {"gray"sv, from_rgb(0xbebebe)},
    {"light"sv, from_rgb(0xd3d3d3)},         {"light"sv, from_rgb(0xd3d3d3)},
    {"dark"sv, from_rgb(0xa0a0a0)},          {"slategray"sv, from_rgb(0xa0b6cd)},
    {"gray0"sv, from_rgb(0x000000)},         {"gray10"sv, from_rgb(0x1a1a1a)},
    {"gray20"sv, from_rgb(0x333333)},        {"gray30"sv, from_rgb(0x4d4d4d)},
    {"gray40"sv, from_rgb(0x666666)},        {"gray50"sv, from_rgb(0x7f7f7f)},
    {"gray60"sv, from_rgb(0x999999)},        {"gray70"sv, from_rgb(0xb3b3b3)},
    {"gray80"sv, from_rgb(0xcccccc)},        {"gray90"sv, from_rgb(0xe5e5e5)},
    {"gray100"sv, from_rgb(0xffffff)}};
} // namespace

namespace explot
{
std::optional<glm::vec4> get_named_color(std::string_view name)
{
  for (const auto &c : named_colors)
  {
    if (name == c.first)
    {
      return c.second;
    }
  }
  return std::nullopt;
}
} // namespace explot
