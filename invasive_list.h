#pragma once
namespace rapid {

template <class T, auto GetNext, auto SetNext>
class InvasiveForwardLinkedList {
  T* m_p;

 public:
  InvasiveForwardLinkedList() : m_p(nullptr) {}
  bool empty() { return m_p == nullptr; }
  void insert(T* p) {
    if (m_p == nullptr) {
      SetNext(p, nullptr);
    } else {
      SetNext(p, m_p);
    }
    m_p = p;
  }
  T* head() { return m_p; }
  template <class TFunc>
  void for_each(TFunc&& f) {
    while (m_p != nullptr) {
      T* nextp = GetNext(m_p);
      if (f(p)) m_p = nextp;
    }
    if (m_p == nullptr) return;
    T *pre = m_p, *p = GetNext(m_p);
    while (p != nullptr) {
      T* next = GetNext(p);
      if (f(p)) {
        SetNext(pre, next);
      } else {
        pre = p;
      }
      p = next;
    }
  }
};
}  // namespace rapid