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
  static void ThrowException(uint64_t id, Handle<String> info);
  static Handle<Exception> GetException();
  static bool HasException();
  static Handle<Object> CallFunction(Handle<FunctionData> fd,
                                     const Parameters &param);
  static void RegisterModule(Handle<String> name, Handle<Object> md);
};


}  // namespace internal
}  // namespace rapid