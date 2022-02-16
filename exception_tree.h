/*
关于异常处理：
在脚本中：
  通过throw(id[,info])抛出一个异常，try{}catch(e,id){}catch(e){}捕获异常 
在C++中：
  通过Executer::ThrowException()抛出异常，然后返回value_failure_exception表示函数因异常而执行失败
  如果Executer::ThrowException()调用时已存在异常（上一次异常未被处理），立即终止程序
*/


#pragma once
#include "handle.h"
#include "list.h"
#include "object.h"
namespace rapid {
namespace internal {

class ExceptionTree {
 public:
  static constexpr uint64_t ID_EXCEPTION = 0;
  static constexpr uint64_t ID_SYNTAX_ERROR = 1;
  static constexpr uint64_t ID_TYPE_ERROR = 2;
  static uint64_t Register(uint64_t parent_id, Handle<String> type);
  static Handle<String> GetType(uint64_t id);
  static bool IsSubexceptionOf(uint64_t id, uint64_t parent_id);

 public:
  static ExceptionTree* Create();
  static void TraceMember(GCTracer* gct);
};

// class Exception : public NativeObjectBase {
// public:
//  Exception(int64_t id, Handle<String> type, Handle<String> info)
//      : NativeObjectBase(2) {
//
//  }
// public:
//  OVERRIDE_UINQUE_ID(Exception)
//};

// inline Handle<Table> NewException(uint64_t id) {
//  Handle<FixedArray> fa = Factory::NewFixedArray(3);
//  fa->set(0, *Factory::NewString("id"));
//  fa->set(1, *Factory::NewString("type"));
//  fa->set(2, *Factory::NewString("info"));
//  return Factory::NewTable(Factory::NewTableInfo(fa));
//}

}  // namespace internal
}  // namespace rapid