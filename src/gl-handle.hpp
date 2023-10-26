#pragma once

#include <GL/glew.h>
#include <cassert>

namespace explot
{
using gl_id = GLuint;

template <void (*delete_handle)(gl_id)>
class gl_handle final
{
  gl_id id = 0;

public:
  gl_handle() = default;
  explicit gl_handle(gl_id id) noexcept : id(id) {}

  gl_handle(const gl_handle &) = delete;
  gl_handle &operator=(const gl_handle &) = delete;

  gl_handle(gl_handle &&other) noexcept : id(other.id) { other.id = 0; }

  gl_handle &operator=(gl_handle &&other) noexcept
  {
    delete_handle(this->id);
    this->id = other.id;
    other.id = 0;
    return *this;
  }

  void reset(gl_id id = 0) noexcept
  {
    delete_handle(this->id);
    this->id = id;
  }

  ~gl_handle() noexcept { delete_handle(this->id); }

  operator gl_id() const noexcept
  {
    assert(id != 0);
    return id;
  }
};

namespace detail
{
inline void delete_vao(gl_id id) { glDeleteVertexArrays(1, &id); }

inline void delete_vbo(gl_id id) { glDeleteBuffers(1, &id); }

inline void delete_program(gl_id id) { glDeleteProgram(id); }

inline void delete_texture(gl_id id) { glDeleteTextures(1, &id); }

inline void delete_fbo(gl_id id) { glDeleteFramebuffers(1, &id); }
} // namespace detail

using vao_handle = gl_handle<detail::delete_vao>;
inline vao_handle make_vao()
{
  gl_id id;
  glGenVertexArrays(1, &id);
  return vao_handle(id);
}

using vbo_handle = gl_handle<detail::delete_vbo>;
inline vbo_handle make_vbo()
{
  gl_id id;
  glGenBuffers(1, &id);
  return vbo_handle(id);
}

using program_handle = gl_handle<detail::delete_program>;
inline program_handle make_program() { return program_handle(glCreateProgram()); }

using texture_handle = gl_handle<detail::delete_texture>;
inline texture_handle make_texture()
{
  gl_id id;
  glGenTextures(1, &id);
  return texture_handle(id);
}

using fbo_handle = gl_handle<detail::delete_fbo>;
inline fbo_handle make_fbo()
{
  gl_id id;
  glGenFramebuffers(1, &id);
  return fbo_handle(id);
}
} // namespace explot
