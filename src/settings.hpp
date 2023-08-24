#pragma once
#include <cstddef>
#include <span>
#include <string>

namespace explot
{
namespace settings
{
struct samples_setting final
{
  std::size_t x = 100;
  std::size_t y = 100;
};

std::string show(std::span<const std::string> path);
bool set(std::span<const std::string> path, std::string_view value);

samples_setting samples();
samples_setting isosamples();

bool parametric();

namespace datafile
{
char separator();
}
} // namespace settings
} // namespace explot
