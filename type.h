#pragma once
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
typedef Object *(*NativeFunctionPtr)(void *data, const Parameters &params);
typedef void (*NativeFuncObjGCCallback)(NativeFunctionPtr ptr, void *data);

typedef Object *(*GetMember)(Object *obj, String *name);
typedef Object *(*SetMember)(Object *obj, String *name, Object *val);
typedef Object *(*GetIndex)(Object *obj, Object *index);
typedef Object *(*SetIndex)(Object *obj, Object *index, Object *val);
typedef void (*TraceRef)(Object *obj, GCTracer *);

struct ObjectInterface {
  GetMember get_member;
  SetMember set_member;
  GetIndex get_index;
  SetIndex set_index;
  TraceRef trace_ref;
};

}  // namespace internal
}  // namespace rapid