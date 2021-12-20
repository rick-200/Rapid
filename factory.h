#pragma once
#include <memory>

#include "handle.h"
#include "heap.h"
#include "object.h"
namespace rapid {
namespace internal {
#define CALL_HEAP_ALLOC(_t, ...)                           \
  {                                                        \
    _t* __pobj_ = Heap::Alloc##_t(__VA_ARGS__);            \
    if (!__pobj_->IsFailure()) return Handle<_t>(__pobj_); \
    if (Failure::cast(__pobj_) != Failure::RetryAfterGC)   \
      return Handle<_t>(__pobj_);                          \
    Heap::DoGC();                                          \
    __pobj_ = Heap::Alloc##_t(__VA_ARGS__);                \
    if (__pobj_->IsFailure()) VERIFY(0);                   \
    return Handle<_t>(__pobj_);                            \
  }
class Factory {
 public:
  static Handle<Integer> NewInteger(int64_t x) {
    return Handle<Integer>(Integer::FromInt64(x));
  }
  static Handle<Float> NewFloat(double x) {
    return Handle<Float>(Float::FromDouble(x));
  }
  static Handle<Object> NewBool(bool x) {
    return x ? TrueValue() : FalseValue();
  }
  static Handle<Object> NullValue() {
    return Handle<Object>(Heap::NullValue());
  }
  static Handle<Object> TrueValue() {
    return Handle<Object>(Heap::TrueValue());
  }
  static Handle<Object> FalseValue() {
    return Handle<Object>(Heap::FalseValue());
  }
  static Handle<String> NewString(const char* cstr, size_t len) {
    CALL_HEAP_ALLOC(String, cstr, len);
  }
  static Handle<String> NewString(const char* cstr) {
    return NewString(cstr, strlen(cstr));
  }
  static Handle<Array> NewArray() { CALL_HEAP_ALLOC(Array); }
  static Handle<Table> NewTable() { CALL_HEAP_ALLOC(Table); }
  static Handle<FixedArray> NewFixedArray(size_t length) {
    CALL_HEAP_ALLOC(FixedArray, length);
  }
  static Handle<InstructionArray> NewInstructionArray(size_t length) {
    CALL_HEAP_ALLOC(InstructionArray, length);
  }
  static Handle<FixedTable> NewFixedTable(size_t size) {
    CALL_HEAP_ALLOC(FixedTable, size);
  }
#define DEF_NEW_STRUCT(_t) \
  static Handle<_t> New##_t() { CALL_HEAP_ALLOC(_t); }
  ITER_STRUCT_DERIVED(DEF_NEW_STRUCT)

};  // namespace internal

}  // namespace internal
}  // namespace rapid