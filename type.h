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

enum class AccessSpecifier {
  Public,
  Private,
  Protected,
};
enum class MetaFunctionID {
  GET_INDEX,
  SET_INDEX,
  // TODO: ADD,DEC,MUL,...
};
typedef Object *(*GetMemberFunction)(Object *obj, String *name,
                                     AccessSpecifier spec);
typedef Object *(*InvokeMemberFunction)(Object *obj, String *name,
                                        const Parameters &params);
typedef Object *(*GetProperty)(Object *obj, String *name, AccessSpecifier spec);
typedef Object *(*SetProperty)(Object *obj, String *name, Object *val,
                               AccessSpecifier spec);
typedef Object *(*GetIndex)(Object *obj, Object *index);
typedef Object *(*SetIndex)(Object *obj, Object *index, Object *val);
typedef void (*TraceRef)(Object *obj, GCTracer *);
typedef Object *(*GetMetaFunction)(Object *obj, MetaFunctionID id);
typedef Object *(*InvokeMetaFunction)(Object *obj, MetaFunctionID id,
                                      const Parameters &params);
/*
由于返回一个NativeFunction需要动态内存分配并增加gc压力
不应使用get_memberfunc和get_metafunction
而应使用invoke_memberfunc和invoke_metafunc
并特殊处理RapidObject

*/
struct ObjectInterface {
  GetProperty get_property;
  SetProperty set_property;
  // GetMemberFunction get_memberfunc;
  // GetMetaFunction get_metafunction;
  InvokeMemberFunction invoke_memberfunc;
  InvokeMetaFunction invoke_metafunc;
  TraceRef trace_ref;
};

}  // namespace internal
}  // namespace rapid