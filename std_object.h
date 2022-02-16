/*
定义了标准原生类型
Array和Dictionary
*/

#pragma once
#include "factory.h"
#include "handle.h"
#include "object.h"
namespace rapid {
namespace internal {

//class Array : public NativeObjectBase {
//  size_t m_length;
//
// private:
//  FixedArray* array() { return FixedArray::cast(GetSlot(0)); }
//  void set_array(FixedArray* array) { SetSlot(0, array); }
//
// private:
//  void change_capacity(size_t new_cap) {
//    HandleScope hs;
//    Handle<FixedArray> new_array = Factory::NewFixedArray(new_cap);
//    memcpy(new_array->begin(), array()->begin(), sizeof(Object*) * m_length);
//    set_array(*new_array);
//  }
//
// public:
//  Array() : NativeObjectBase(1), m_length(0) {
//    HandleScope hs;
//    Handle<FixedArray> new_array = Factory::NewFixedArray(4);
//    set_array(*new_array);
//  }
//  size_t length() { return m_length; }
//  size_t capacity() { return array()->length(); }
//  void reserve(size_t size) {
//    if (size <= capacity()) return;
//    if (size < capacity() * 2) size = capacity() * 2;
//    change_capacity(size);
//  }
//  void push(Handle<Object> v) {
//    reserve(m_length + 1);
//    array()->set(++m_length, v.ptr());
//  }
//  void pop() {
//    --m_length;
//    array()->set(m_length, nullptr);
//  }
//  void shrink_to_fit() {
//    if (capacity() != m_length) change_capacity(m_length);
//  }
//  void resize(size_t size) {
//    if (size == m_length) return;
//    if (size > m_length) {
//      reserve(size);
//      for (size_t i = m_length; i < size; i++) array()->set(i, nullptr);
//      m_length = size;
//    } else {
//      for (size_t i = size; i < m_length; i++) array()->set(i, nullptr);
//      m_length = size;
//      if (capacity() > m_length * 3) {
//        change_capacity(m_length * 2);
//      }
//    }
//  }
//  void set(size_t pos, Handle<Object> obj) { array()->set(pos, *obj); }
//  Object* get(size_t idx) { return array()->get(idx); }
//  virtual void UniqueID() override {}
//};

}  // namespace internal
}  // namespace rapid