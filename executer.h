#pragma once
#include "handle.h"
#include "list.h"
#include "object.h"
#include "type.h"
namespace rapid {
namespace internal {

class Executer : public StaticClass {
 public:
  static Executer *Create();
  static void Destory(Executer *p);
  static void TraceStack(GCTracer *gct);
  static void ThrowException(Handle<Exception>);
  static Handle<Exception> GetException();
  static bool HasException();
  static Handle<Object> CallFunction(Handle<SharedFunctionData> sfd,
                                     const Parameters &param);
  static Handle<Object> CallFunction(Handle<FunctionData> fd,
                                     const Parameters &param);
};
class RawParameters {
  Object **m_p_raw;
  size_t m_cnt;
  Object *m_this;

 public:
  RawParameters(Object *this_obj, Object **p, size_t cnt)
      : m_this(this_obj), m_p_raw(p), m_cnt(cnt) {}

 public:
  Object *get(size_t pos) const {
    ASSERT(pos < m_cnt);
    return m_p_raw[pos];
  }
  Object *get_this() const { return m_this; }
  Object *operator[](size_t pos) const { return get(pos); }
  size_t count() const { return m_cnt; }
};
class Parameters : public RawParameters {
 public:
  Parameters(Handle<Object> this_obj, Handle<Object> *p, size_t cnt)
      : RawParameters(this_obj.ptr(), reinterpret_cast<Object **>(p), cnt) {
#ifdef _DEBUG
    for (size_t i = 0; i < cnt; i++)
      ASSERT(RawParameters::get(i) == p[i].ptr());
#endif
  }
  Parameters(Handle<Object> this_obj, const List<Handle<Object>> &list)
      : Parameters(this_obj, list.begin(), list.size()) {}
  Parameters(Object *this_obj, Object **p, size_t cnt)
      : RawParameters(this_obj, p, cnt) {}

 public:
  Handle<Object> get(size_t pos) const {
    return Handle<Object>(RawParameters::get(pos), non_local);
  }
  Handle<Object> get_this() const {
    return Handle<Object>(RawParameters::get_this());
  }
  Handle<Object> operator[](size_t pos) const { return get(pos); }
};

}  // namespace internal
}  // namespace rapid