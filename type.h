#pragma once
#include "uuid.h"
namespace rapid {

typedef uint8_t byte;
typedef uint8_t Cmd;
class StaticClass {
 public:
  StaticClass() = delete;
  StaticClass(const StaticClass &) = delete;
  StaticClass(StaticClass &&) = delete;
  StaticClass &operator=(const StaticClass &) = delete;
  StaticClass &operator=(StaticClass &&) = delete;
};

namespace internal {

class Parameters;
class String;
class Object;
class GCTracer;
//typedef Handle<Object> (*GetProperty)(void *obj, Handle<String> name);
//typedef Handle<Object> (*SetProperty)(void *obj, Handle<String> name,
//                                      Handle<Object> value);
//typedef Handle<Object> (*Invoke)(void *obj, Handle<Object> params);
//struct NativeObjectInterface;
//typedef void (*FreeNativeObject)(NativeObjectInterface *info, void *data);
//struct NativeObjectInterface {
//  UUID uuid;
//  GetProperty get_prop;
//  SetProperty set_prop;
//  Invoke invoke;
//  FreeNativeObject free;
//};

}  // namespace internal
}  // namespace rapid