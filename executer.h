#pragma once
#include "handle.h"
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
  // void CallRapidFunc(Handle<FunctionData> func) {}
  // void CallRapidFunc(Handle<Object> func) {}
  static Handle<Exception> GetException();
  static bool HasException();
};

} // namespace internal
} // namespace rapid