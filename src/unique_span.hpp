#pragma once

#include <memory>
#include <span>
#include <utility>

namespace explot
{
template <typename T>
struct unique_span final
{
  std::unique_ptr<T[]> data;
  std::size_t size;

  operator std::span<T>() noexcept { return {data.get(), size}; }
  operator std::span<const T>() const noexcept { return {data.get(), size}; }

  T *get() noexcept { return data.get(); }
  const T *get() const noexcept { return data.get(); }

  T &operator[](std::size_t i) noexcept { return data[i]; }
  const T &operator[](std::size_t i) const noexcept { return data[i]; }

  unique_span() noexcept : data(), size(0) {}
  unique_span(unique_span &&other) noexcept
      : data(std::move(other.data)), size(std::exchange(other.size, 0))
  {
  }
  unique_span(std::unique_ptr<T[]> data, size_t size) noexcept : data(std::move(data)), size(size)
  {
  }
  unique_span(T *data, size_t size) noexcept : data(data), size(size) {}
  unique_span(size_t n) : data(std::make_unique<T[]>(n)), size(n) {}

  friend T *begin(unique_span &s) noexcept { return s.get(); }
  friend const T *begin(const unique_span &s) noexcept { return s.get(); }
  friend T *end(unique_span &s) noexcept { return s.get() + s.size; }
  friend const T *end(const unique_span &s) noexcept { return s.get() + s.size; }
};

template <typename T>
unique_span<T> make_unique_span(size_t size)
{
  return {size};
}

} // namespace explot
