#include "object.h"

#include "executer.h"
#include "factory.h"
#include "handle.h"
namespace rapid {
namespace internal {
void Array::change_capacity(size_t new_cap) {
  Handle<FixedArray> h = Factory::NewFixedArray(new_cap);
  for (size_t i = 0; i < m_length; i++) h->set(i, m_array->get(i));
  m_array = h.ptr();
}
// Array implement --------------------------------------
Object* Array::invoke_metafunction(Object* obj, MetaFunctionID id,
                                   const Parameters& params) {
  Array* _this = Array::cast(obj);
  if (id == MetaFunctionID::GET_INDEX) {
    ASSERT(params.count() == 1 && params[0]->IsInteger());
    int64_t pos = Integer::cast(params[0].ptr())->value();
    if (pos < 0 || pos >= _this->length()) VERIFY(0);
    return _this->get(pos);
  } else if (id == MetaFunctionID::SET_INDEX) {
    ASSERT(params.count() == 2 && params[0]->IsInteger());
    int64_t pos = Integer::cast(params[0].ptr())->value();
    if (pos < 0 || pos >= _this->length()) VERIFY(0);
    _this->set(pos, params[1].ptr());
    return Heap::NullValue();
  } else {
    return Failure::NotImplemented;
  }
  return nullptr;
}
Object* Array::invoke_memberfunc(Object* obj, String* name,
                                 const Parameters& params) {
  const RawParameters rp = params;
  Array* _this = Array::cast(obj);
  if (String::Equal(name, "push")) {
    if (rp.count() != 1) VERIFY(0);
    _this->push(rp[0]);
    return Heap::NullValue();
  } else if (String::Equal(name, "reserve")) {
    if (rp.count() != 1 || rp[0]->IsInteger()) VERIFY(0);
    _this->reserve(Integer::cast(rp[0])->value());
    return Heap::NullValue();
  }
  return Failure::NotImplemented;
}
//---------------------------------------------------------

void Table::rehash_expand() {
  Handle<FixedTable> h = Factory::NewFixedTable(m_table->size() << 1);
  m_table->rehash_to(h.ptr());
  m_table = h.ptr();
}

void Table::try_rehash_shrink() {
  if (m_table->used() > (m_table->size() >> 3 /* size/8 */)) return;
  Handle<FixedTable> h = Factory::NewFixedTable(m_table->used() << 2);
  m_table->rehash_to(h.ptr());
  m_table = h.ptr();
}

Object* Object::get_property(Object* obj, String* name, AccessSpecifier spec) {
  return Failure::NotImplemented;
}

Object* Object::set_property(Object* obj, String* name, Object* val,
                             AccessSpecifier spec) {
  return Failure::NotImplemented;
}

// Object* Object::get_metafunction(Object* obj, MetaFunctionID id) {
//  return Failure::NotImplemented;
//}
//
// Object* Object::get_memberfunc(Object* obj, String* name,
//                               AccessSpecifier spec) {
//  return Failure::NotImplemented;
//}

Object* Object::invoke_metafunction(Object* obj, MetaFunctionID id,
                                    const Parameters& params) {
  return Failure::NotImplemented;
}
Object* Object::invoke_memberfunc(Object* obj, String* name,
                                  const Parameters& params) {
  return Failure::NotImplemented;
}

void Object::trace_ref(Object* obj, GCTracer* gct) {}

void debug_print(FILE* f, Object* obj) {
  if (obj->IsInteger()) {
    fprintf(f, "<int>%lld", Integer::cast(obj)->value());
  } else if (obj->IsFloat()) {
    fprintf(f, "<float>%g", Float::cast(obj)->value());
  } else if (obj->IsString()) {
    fprintf(f, "<str>%s", String::cast(obj)->cstr());
  } else if (obj->IsBool()) {
    fprintf(f, "<bool>%s", obj->IsTrue() ? "true" : "false");
  } else if (obj->IsNull()) {
    fprintf(f, "<null>");
  } else if (obj->IsArray()) {
    fprintf(f, "<array>");
  } else if (obj->IsFixedArray()) {
    fprintf(f, "<finedarray>");
  } else if (obj->IsTable()) {
    fprintf(f, "<table>");
  } else if (obj->IsFixedTable()) {
    fprintf(f, "<finedtable>");
  } else if (obj->IsFunctionData()) {
    fprintf(f, "<funcdata>%p:%s@%p", FunctionData::cast(obj),
            FunctionData::cast(obj)->shared_data->name->cstr(),
            FunctionData::cast(obj)->shared_data);
  } else if (obj->IsSharedFunctionData()) {
    fprintf(f, "<sharedfuncdata>%s@%p",
            SharedFunctionData::cast(obj)->name->cstr(),
            SharedFunctionData::cast(obj));
  } else if (obj->IsNativeObject()) {
    fprintf(f, "<native_obj>%p", NativeObject::cast(obj));
  } else {
    fprintf(f, "<unknown>");
  }
}

}  // namespace internal
}  // namespace rapid
