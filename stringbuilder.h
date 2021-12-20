#pragma once
#include "factory.h"
#include "list.h"
namespace rapid {
namespace internal {

class StringBuilder {
  List<char> m_v;

 public:
  StringBuilder() {}
  Handle<String> ToString() {
    return Factory::NewString(m_v.begin(), m_v.size());
  }
  template <typename... TArgs>
  StringBuilder& AppendFormat(const char* fmt, TArgs... args) {
    size_t len = snprintf(nullptr, 0, fmt, args...);
    size_t siz = m_v.size();
    m_v.resize(siz + len + 1);
    snprintf(&m_v[siz], len + 1 /* '\0' */, fmt, args...);
    m_v.pop();  // 删去 '\0'
    return *this;
  }
  StringBuilder& Append(const char* s, size_t len) {
    size_t siz = m_v.size();
    m_v.resize(siz + len);
    memcpy(&m_v[siz], s, len * sizeof(char));
    return *this;
  }
  StringBuilder& Append(const char* s) { return Append(s, strlen(s)); }
  StringBuilder& Append(int64_t v) { return AppendFormat("%lld", v); }
  StringBuilder& Append(double v) { return AppendFormat("%g", v); }
  StringBuilder& Append(Handle<String> v) {
    return AppendFormat("%s", v->cstr());
  }
  StringBuilder& Append(bool v) { return v ? Append("true") : Append("false"); }
  StringBuilder& Append(Handle<Object> v) {
    if (v->IsString()) {
      Append(Handle<String>::cast(v)->cstr());
    } else if (v->IsInteger()) {
      Append(Handle<Integer>::cast(v)->value());
    } else if (v->IsFloat()) {
      Append(Handle<Float>::cast(v)->value());
    } else if (v->IsBool()) {
      Append(v->IsTrue());
    } else {
      Append("{object}");
    }
    return *this;
  }
};

}  // namespace internal
}  // namespace rapid