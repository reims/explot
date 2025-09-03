#include "prefix_sum.hpp"
#include "program.hpp"
// #include <fmt/format.h>
// #include <fstream>

namespace
{
using namespace explot;

static constexpr auto first_pass_shader = R"(#version 430 core

layout(local_size_x = 1024) in;

layout(std430, binding=0) buffer block1
{
  float data[];
};

uniform uint stride;

shared float work_space1[gl_WorkGroupSize.x];
shared float work_space2[gl_WorkGroupSize.x];

void main()
{
  work_space1[gl_LocalInvocationIndex] = data[gl_GlobalInvocationID.x * stride];

  memoryBarrierShared();

  uint i = 0;

  while (i < 10)
  {
    uint offset = 1 << i;
    work_space2[gl_LocalInvocationIndex] = work_space1[gl_LocalInvocationIndex] + (gl_LocalInvocationIndex >= offset ? work_space1[gl_LocalInvocationIndex - offset] : 0.0);
    memoryBarrierShared();
    i += 1;
    offset = 1 << i;
    work_space1[gl_LocalInvocationIndex] = work_space2[gl_LocalInvocationIndex] + (gl_LocalInvocationIndex >= offset ? work_space2[gl_LocalInvocationIndex - offset] : 0.0);
    memoryBarrierShared();
    i += 1;
  }

  data[gl_GlobalInvocationID.x * stride] = work_space1[gl_LocalInvocationIndex];
}
)";

static constexpr auto second_pass_shader = R"(#version 430 core
					      
layout(local_size_x = 1023) in;

layout(std430, binding=0) buffer block1
{
  float data[];
};

uniform uint stride;

void main()
{
  uint start = (stride << 10) * (gl_WorkGroupID.x + 1);
  uint offset_index =  start - 1;
  data[start + stride * gl_LocalInvocationIndex] += data[offset_index]; 
}
)";

static constexpr auto sequential_pass_shader = R"(#version 430 core

layout(local_size_x = 1) in;

layout(std430, binding=0) buffer block1
{
  float data[];
};

uniform uint start;
uniform uint stride;
uniform uint count;

void main()
{
  for (uint i = 0; i < count; ++i)
  {
    uint idx = start + stride * i;
    data[idx] += data[idx - stride];
    memoryBarrier();
  }
}
)";

static constexpr auto mask = (1u << 10) - 1u;

program_handle make_first_pass_program() { return make_compute_program(first_pass_shader); }

program_handle make_second_pass_program() { return make_compute_program(second_pass_shader); }

program_handle make_sequential_pass_program()
{
  return make_compute_program(sequential_pass_shader);
}

void first_pass(gl_id prog, uint32_t stride, uint32_t count)
{
  glUseProgram(prog);
  glUniform1ui(glGetUniformLocation(prog, "stride"), stride);
  auto workGroups = count >> 10;
  if (workGroups > 0)
  {
    glDispatchCompute(workGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }
}
void second_pass(gl_id prog, uint32_t stride, uint32_t count)
{
  glUseProgram(prog);
  glUniform1ui(glGetUniformLocation(prog, "stride"), stride);
  if (count >> 10 > 1)
  {
    auto workGroups = (count >> 10) - 1;
    glDispatchCompute(workGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }
}
void sequential_pass(gl_id prog, uint32_t start, uint32_t stride, uint32_t count)
{
  if (count > 1)
  {
    if (start == 0)
    {
      start = 1;
      count -= 1;
    }
    glUseProgram(prog);
    uniform ufs[] = {{"stride", stride}, {"count", count}, {"start", start}};
    set_uniforms(prog, ufs);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }
}

void prefix_sum_impl(gl_id first_pass_prog, gl_id second_pass_prog, gl_id sequential_pass_prog,
                     uint32_t stride, uint32_t count)
{
  first_pass(first_pass_prog, stride, count);
  if (count > mask)
  {
    prefix_sum_impl(first_pass_prog, second_pass_prog, sequential_pass_prog, stride << 10,
                    count >> 10);
  }
  second_pass(second_pass_prog, stride, count);
  const auto rest = count & mask;
  sequential_pass(sequential_pass_prog, count - rest, stride, rest);
}
} // namespace

namespace explot
{
void prefix_sum(gl_id data, uint32_t count)
{
  auto first_pass_program = make_first_pass_program();
  auto second_pass_program = make_second_pass_program();
  auto sequential_pass_program = make_sequential_pass_program();
  auto vao = make_vao();
  glBindVertexArray(vao);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, data);
  prefix_sum_impl(first_pass_program, second_pass_program, sequential_pass_program, 1u, count);
}
} // namespace explot
