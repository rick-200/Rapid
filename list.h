#pragma once
#include <cstring>
#include <new>
#include <utility>

#include "allocation.h"
#include "preprocessors.h"
namespace rapid {
namespace internal {
template <class T>
class List : public Malloced {
  static_assert(std::is_trivially_destructible_v<T>,
                "List<T>: T must be trivially destructible.");
  T *m_p;
  size_t m_siz, m_cap;

 private:
  void change_capacity(size_t new_cap) {
    T *oldp = m_p;
    m_p = (T *)Alloc(sizeof(T) * new_cap);
    VERIFY(m_p != nullptr);
    memcpy(m_p, oldp, sizeof(T) * m_siz);
    Free(oldp);
    m_cap = new_cap;
  }

 public:
  List() : m_p(nullptr), m_siz(0), m_cap(0) {}
  ~List() { delete[] m_p; }
  T *begin() const { return m_p; }
  T *end() const { return m_p + m_siz; }
  size_t size() const { return m_siz; }
  bool empty() { return m_siz == 0; }
  size_t capacity() const { return m_cap; }
  T &operator[](size_t pos) { return m_p[pos]; }
  const T &operator[](size_t pos) const { return m_p[pos]; }
  void push(const T &v) {
    reserve(m_siz + 1);
    m_p[m_siz++] = v;
  }
  void pop() {
    ASSERT(m_siz > 0);
    --m_siz;
  }
  void resize(size_t new_size, const T val = {}) {
    if (new_size < m_siz) {
      m_siz = new_size;
      return;
    }
    reserve(new_size);
    for (size_t i = m_siz; i < new_size; i++) {
      m_p[i] = val;
    }
    m_siz = new_size;
  }
  void reserve(size_t size) {
    if (size <= m_cap) return;
    change_capacity(std::max(size, m_cap << 1));
  }
  void shrink_to_fit() { change_capacity(m_siz); }
  void clear() { m_siz = 0; }
  const T &front() const { return *m_p; }
  const T &back() const { return m_p[m_siz - 1]; }
  T &front() { return *m_p; }
  T &back() { return m_p[m_siz - 1]; }
};
template <class T>
class ListView {
  T *m_p;
  size_t m_siz;

 public:
  ListView(const List<T> &list) : m_p(list.begin()), m_siz(list.length()) {}
  ListView(T *p, size_t siz) : m_p(p), m_siz(siz) {}
  T &operator[](size_t pos) {
    ASSERT(pos < m_siz);
    return m_p[pos];
  }
  const T &operator[](size_t pos) const {
    ASSERT(pos < m_siz);
    return m_p[pos];
  }
};

}  // namespace internal
}  // namespace rapid