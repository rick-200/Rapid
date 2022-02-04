#pragma once
#include "handle.h"
#include "object.h"
#include "type.h"
namespace rapid {
namespace internal {
template <class Derived>
class NativeModuleBase {
 protected:
  Object* get_property(Object* obj, String* name, AccessSpecifier spec) {
    return Failure::PropertyNotFound;
  }
  Object* set_property(Object* obj, String* name, Object* val,
                       AccessSpecifier spec) {
    return Failure::PropertyNotFound;
  }
  Object* invoke_memberfunc(Object* obj, String* name,
                            const Parameters& params) {
    return Failure::PropertyNotFound;
  }
  Object* invoke_metafunc(Object* obj, MetaFunctionID id,
                          const Parameters& params) {
    return Failure::NotImplemented;
  }
  void trace_ref(Object* obj, GCTracer*) {}
};
template <class Derived>
Object* module_get_property(Object* obj, String* name, AccessSpecifier spec) {
  //NativeObject::cast(obj)->m_data
  return static_cast<Derived*>(this)->get_property(obj, name, spec);
}
template <class T>
Handle<Object> CreateNativeModule() {}
}  // namespace internal
}  // namespace rapid