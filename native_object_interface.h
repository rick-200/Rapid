#pragma once
#include <cstdint>

#include "factory.h"
#include "uuid.h"
namespace rapid {
namespace internal {

//用于从脚本调用C++，参数保存在脚本栈中
class Parameters {
  Object** m_p;
  size_t m_count;

 public:
  Parameters(Object** p, size_t count) : m_p(p), m_count(count) {}
  Object* operator[](size_t pos) const { return m_p[pos]; }
  size_t count() const { return m_count; }
};

typedef Object* (*GetPropertyFunction)(void* obj, String* name);
typedef Object* (*SetPropertyFunction)(void* obj, String* name, Object* value);
typedef Object* (*InvokeSelfFunction)(void* obj, const Parameters& params);
struct NativeObjectInterface;
typedef void (*FreeObject)(void* data, NativeObjectInterface* info);
struct NativeObjectInterface {
  UUID uuid;
  GetPropertyFunction get_prop;
  SetPropertyFunction set_prop;
  InvokeSelfFunction invoke;
  FreeObject free;
};
typedef Object* (*NativeFunctionPtr)(const Parameters& params);
inline Object* _NativeFunctionInvoker(void* obj, const Parameters& params) {
  return static_cast<NativeFunctionPtr>(obj)(params);
}
static constexpr NativeObjectInterface _NativeFunctionInterface = {
    {0x17EF779BAC2B4B0, 0x15902275E1A5E078},
    nullptr,
    nullptr,
    _NativeFunctionInvoker,
    nullptr,
};
inline Handle<NativeObject> NewNativeFunction(NativeFunctionPtr fp) {
  return Factory::NewNativeObject(&_NativeFunctionInterface, fp);
}

}  // namespace internal
}  // namespace rapid
