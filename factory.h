#pragma once
#include <memory>

#include "exception_tree.h"
#include "handle.h"
#include "heap.h"
#include "object.h"
namespace rapid {
namespace internal {

#if DEBUG_ALWAYS_GC
#define DEBUG_GC Heap::DoGC()
#else
#define DEBUG_GC ((void)0)
#endif  // DEBUG_ALWAYS_GC

#define CALL_HEAP_ALLOC(_t, ...)                        \
  {                                                     \
    DEBUG_GC;                                           \
    if (Heap::NeedGC()) Heap::DoGC();                   \
    _t *__pobj_ = Heap::Alloc##_t(__VA_ARGS__);         \
    if (__pobj_ != nullptr) return Handle<_t>(__pobj_); \
    Heap::DoGC();                                       \
    __pobj_ = Heap::Alloc##_t(__VA_ARGS__);             \
    if (__pobj_ == nullptr) {                           \
      return Handle<_t>();                              \
    }                                                   \
    return Handle<_t>(__pobj_);                         \
  }

class Factory {
 private:
  static Handle<Exception> AllocException() { CALL_HEAP_ALLOC(Exception); }

 public:
  static Handle<Integer> NewInteger(int64_t x) {
    return Handle<Integer>(Integer::FromInt64(x));
  }
  static Handle<Float> NewFloat(double x) {
    return Handle<Float>(Float::FromDouble(x));
  }
  static Handle<Object> NewBool(bool x) {
    return Handle<Object>(x ? value_true : value_false);
  }
  static Handle<String> NewString(const char *cstr, size_t len) {
    CALL_HEAP_ALLOC(String, cstr, len);
  }
  static Handle<String> NewString(const char *cstr) {
    return NewString(cstr, strlen(cstr));
  }
  static Handle<FixedArray> NewFixedArray(size_t length) {
    CALL_HEAP_ALLOC(FixedArray, length);
  }
  static Handle<FixedDictionary> NewFixedDictionary(size_t size) {
    CALL_HEAP_ALLOC(FixedDictionary, size);
  }
  static Handle<Array> NewArray() { CALL_HEAP_ALLOC(Array); }
  static Handle<Dictionary> NewDictionary() { CALL_HEAP_ALLOC(Dictionary); }
  static Handle<InstructionArray> NewInstructionArray(size_t length) {
    CALL_HEAP_ALLOC(InstructionArray, length);
  }
  static Handle<VarInfoArray> NewVarInfoArray(size_t length) {
    CALL_HEAP_ALLOC(VarInfoArray, length);
  }
  static Handle<ExternVarInfoArray> NewExternVarInfoArray(size_t length) {
    CALL_HEAP_ALLOC(ExternVarInfoArray, length);
  }
  static Handle<ExternVar> NewExternVar() { CALL_HEAP_ALLOC(ExternVar); }
  static Handle<SharedFunctionData> NewSharedFunctionData() {
    CALL_HEAP_ALLOC(SharedFunctionData);
  }
  static Handle<FunctionData> NewFunctionData() {
    CALL_HEAP_ALLOC(FunctionData);
  }
  static Handle<StackTraceData> NewStackTraceData() {
    CALL_HEAP_ALLOC(StackTraceData);
  }
  static Handle<NativeObject> NewNativeObject(
      const NativeObjectInterface *interface, void *data) {
    CALL_HEAP_ALLOC(NativeObject, interface, data);
  }
  static Handle<TableInfo> NewTableInfo(Handle<FixedArray> prop_list) {
    CALL_HEAP_ALLOC(TableInfo, *prop_list);
  }
  static Handle<Table> NewTable(Handle<TableInfo> info) {
    CALL_HEAP_ALLOC(Table, *info);
  }
  static Handle<Exception> NewException(uint64_t id, Handle<String> info,
                                        Handle<String> stack_trace) {
    Handle<Exception> exc = AllocException();
    exc->id = id;
    exc->info = *info;
    exc->stack_trace = *stack_trace;
    return exc;
  }
  static Handle<Object> FalseValue() {
    return Handle<Object>(value_false, non_local);
  }
  static Handle<Object> TrueValue() {
    return Handle<Object>(value_true, non_local);
  }
  // static Handle<Object> FalseValue() {
  //  return Handle<Object>(value_false, non_local);
  //}
};

}  // namespace internal
}  // namespace rapid