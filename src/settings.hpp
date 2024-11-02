#pragma once
#include <cstddef>
#include <span>
#include <string>
#include "line_type.hpp"

namespace explot
{
namespace settings
{
struct samples_setting final
{
  std::size_t x = 100;
  std::size_t y = 100;
};

enum class data_type : char
{
  normal,
  time
};

std::string show(std::span<const std::string> path);
bool set(std::span<const std::string> path, std::string_view value);

samples_setting samples();
samples_setting isosamples();

bool parametric();

const line_type &line_type_by_index(int idx);
const char *timefmt();
data_type xdata();

namespace datafile
{
char separator();
}
} // namespace settings
} // namespace explot
