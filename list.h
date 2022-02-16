#pragma once
#include <cstring>
#include <new>
#include <utility>
#include "util.h"
#include "allocation.h"
#include "preprocessors.h"
namespace rapid {
namespace internal {
template <class T>
class FixedList {
  static_assert(std::is_trivial_v<T>, "FixedList<T> need T to be a trivial type");
  T *m_p;
  size_t m_size;

 public:
  FixedList(size_t size) : m_size(size) {
    m_p = size != 0 ? Allocate<T>(size) : nullptr;
  }
  FixedList(const FixedList &) = delete;
  FixedList(FixedList &&other) : m_p(other.m_p), m_size(other.m_size) {
    other.m_p = nullptr;
    other.m_size = 0;
  }
  FixedList &operator=(const FixedList &) = delete;
  FixedList &operator=(FixedList &&other) {
    if (m_p != nullptr) Free(m_p);
    m_p = other.m_p;
    m_size = other.m_size;
    other.m_p = nullptr;
    other.m_size = 0;
    return *this;
  }
  ~FixedList() {
    if (m_p != nullptr) Free(m_p);
  }
  T *begin() const { return m_p; }
  T *end() const { return m_p + m_size; }
  size_t size() const { return m_size; }
  T &operator[](size_t pos) {
    ASSERT(pos < m_size);
    return m_p[pos];
  }
  const T &operator[](size_t pos) const {
    ASSERT(pos < m_size);
    return m_p[pos];
  }
  const T &front() const { return *m_p; }
  const T &back() const { return m_p[m_size - 1]; }
  T &front() { return *m_p; }
  T &back() { return m_p[m_size - 1]; }
};
template <class T>
class List {
  FixedList<T> m_list;
  size_t m_size;

 private:
  void change_capacity(size_t new_cap) {
    FixedList<T> old = std::move(m_list);
    m_list = FixedList<T>(new_cap);
    memcpy(m_list.begin(), old.begin(), sizeof(T) * m_size);
  }

 public:
  List() : m_list(0), m_size(0) {}
  ~List() = default;
  T *begin() const { return m_list.begin(); }
  T *end() const { return m_list.begin() + m_size; }
  size_t size() const { return m_size; }
  bool empty() { return m_size == 0; }
  size_t capacity() const { return m_list.size(); }
  T &operator[](size_t pos) {
    ASSERT(pos < m_size);
    return m_list[pos];
  }
  const T &operator[](size_t pos) const {
    ASSERT(pos < m_size);
    return m_list[pos];
  }
  void push(const T &v) {
    reserve(m_size + 1);
    m_list[m_size++] = v;
  }
  void pop() {
    ASSERT(m_size > 0);
    --m_size;
  }
  void resize(size_t new_size, const T val = {}) {
    if (new_size < m_size) {
      m_size = new_size;
      return;
    }
    reserve(new_size);
    for (size_t i = m_size; i < new_size; i++) {
      m_list[i] = val;
    }
    m_size = new_size;
  }
  void reserve(size_t size) {
    if (size <= capacity()) return;
    change_capacity(std::max(size, capacity() << 1));
  }
  void shrink_to_fit() { change_capacity(m_size); }
  void clear() { m_size = 0; }
  const T &front() const { return m_list[0]; }
  const T &back() const { return m_list[m_size-1]; }
  T &front() { return m_list[0]; }
  T &back() { return m_list[m_size - 1]; }
};
}  // namespace internal
}  // namespace rapid