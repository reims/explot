#pragma once
#include <string>
#include "line_type.hpp"
#include "commands.hpp"

namespace explot
{
namespace settings
{

std::string show(const show_command &cmd);
bool set(const set_command &cmd);
void unset(const unset_command &cmd);

samples_setting samples();
samples_setting isosamples();

bool parametric();

const line_type &line_type_by_index(uint32_t idx);
const char *timefmt();
data_type xdata();
bool hidden3d();

namespace datafile
{
char separator();
}
} // namespace settings
} // namespace explot
