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
  StringBuilder &AppendFormat(const char *fmt, const TArgs&... args) {
    size_t len = snprintf(nullptr, 0, fmt, args...);
    size_t siz = m_v.size();
    m_v.resize(siz + len + 1);
    snprintf(&m_v[siz], len + 1 /* '\0' */, fmt, args...);
    m_v.pop(); // 删去 '\0'
    return *this;
  }
  StringBuilder &AppendString(const char *s, size_t len) {
    size_t siz = m_v.size();
    m_v.resize(siz + len);
    memcpy(&m_v[siz], s, len * sizeof(char));
    return *this;
  }
  StringBuilder &AppendChar(char c) {
    m_v.push(c);
    return *this;
  }
  StringBuilder &AppendString(const char *s) {
    return AppendString(s, strlen(s));
  }
  StringBuilder &AppendInt(int64_t v) { return AppendFormat("%lld", v); }
  StringBuilder &AppendDouble(double v) { return AppendFormat("%g", v); }
  StringBuilder &AppendString(Handle<String> v) {
    return AppendFormat("%s", v->cstr());
  }
  StringBuilder &AppendBool(bool v) {
    return AppendString(v ? "true" : "false");
  }
  StringBuilder &AppendObject(Handle<Object> v) {
    if (v->IsString()) {
      AppendChar('\'');
      AppendString(Handle<String>::cast(v)->cstr());
      AppendChar('\'');
    } else if (v->IsInteger()) {
      AppendInt(Handle<Integer>::cast(v)->value());
    } else if (v->IsFloat()) {
      AppendDouble(Handle<Float>::cast(v)->value());
    } else if (v->IsBool()) {
      AppendBool(v->IsTrue());
    } else {
      AppendString("{object}");
    }
    return *this;
  }
  template <typename... TArgs>
  static Handle<String> Format(const char *fmt, const TArgs &...args) {
    StringBuilder sb;
    sb.AppendFormat(fmt, args...);
    return sb.ToString();
  }
};

} // namespace internal
} // namespace rapid