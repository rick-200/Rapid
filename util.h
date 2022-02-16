#pragma once
#include <cstdint>
#include <numeric>
#include <type_traits>

namespace rapid {
template <class To, class From>
constexpr bool __can_numeric_convert =
    std::numeric_limits<From>::is_specialized&&
        std::numeric_limits<To>::is_specialized;
template <class To, class From,
          std::enable_if_t<__can_numeric_convert<To, From>, int> = 0>
constexpr To numeric_cast(From val) {
  if constexpr (std::numeric_limits<From>::max() >
                    std::numeric_limits<To>::max() ||
                std::numeric_limits<From>::lowest() <
                    std::numeric_limits<To>::lowest())
    if (val < std::numeric_limits<To>::lowest() ||
        val > std::numeric_limits<To>::max()) {
      fprintf(stderr, "numeric_cast error\n");
      abort();
    }
  return static_cast<To>(val);
}

template <class T, class... TArgs>
inline void Construct(T* p, TArgs&&... args) {
  new (p) T(std::forward<TArgs>(args)...);
}

template <class T>
void Destruct(T* x) {
  if constexpr (!std::is_trivially_destructible_v<T>) {
    x->~T();
  }
}
template <class T>
void DestructArray(T* p, size_t size) {
  if constexpr (!std::is_trivially_destructible_v<T>) {
    for (size_t i = 0; i < size; i++) p[i].~T();
  }
}

template <class T, T* T::*_m_next>
class IntrusiveForwardList {
  T* m_head;
  size_t m_count;

 private:
  static inline T* get_next(T* p) { return p->*_m_next; }
  static inline void set_next(T* p, T* pnext) { p->*_m_next = pnext; }

 public:
  IntrusiveForwardList() : m_head(nullptr), m_count(0) {}
  void push(T* p) {
    if (m_head == nullptr) {
      m_head = p;
      set_next(m_head, nullptr);
    } else {
      set_next(p, m_head);
      m_head = p;
    }
    ++m_count;
  }
  size_t count() const { return m_count; }
  template <class TFunc>
  void for_each(TFunc&& f /*返回true以删除节点*/) {
    if (m_head == nullptr) return;
    T* next = get_next(m_head);
    while (f(m_head)) {
      m_head = next;
      if (m_head == nullptr) return;
      next = get_next(m_head);
    }
    if (m_head == nullptr) return;
    T *pre = m_head, *p = get_next(m_head);
    while (p != nullptr) {
      T* next = get_next(p);
      if (f(p)) {
        set_next(pre, next);
      } else {
        pre = p;
      }
      p = next;
    }
  }
};

constexpr size_t cstring_length(const char* s) {
  const char* ps = s;
  while (*ps != '\0') ++ps;
  return ps - s;
}
constexpr uint64_t hash_string(const char* ps, size_t len) {
  uint64_t ret = 14695981039346656037ULL;
  size_t step = (len >> 8) + 1;  //采样<=256个
  for (size_t i = 0; i < len; i += step) {
    ret ^= ps[i];
    ret *= 1099511628211ULL;
  }
  return ret;
}
struct CStringWrap {
  const char* s;
  size_t length;
  uint64_t hash;
  constexpr CStringWrap(const char* str) : s(str), length(0), hash(0) {
    length = cstring_length(str);
    hash = hash_string(str, length);
  }
};

}  // namespace rapid