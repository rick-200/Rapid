#include "object.h"

#include "factory.h"
#include "handle.h"
namespace rapid {
namespace internal {
void Array::change_capacity(size_t new_cap) {
  Handle<FixedArray> h = Factory::NewFixedArray(new_cap);
  for (size_t i = 0; i < m_length; i++) h->set(i, m_array->get(i));
  m_array = h.ptr();
}
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

Object* Object::get_member(Object* obj, String* name) {
  return Failure::NotImplemented;
}

Object* Object::set_member(Object* obj, String* name, Object* val) {
  return Failure::NotImplemented;
}

Object* Object::get_index(Object* obj, Object* idx) {
  return Failure::NotImplemented;
}

Object* Object::set_index(Object* obj, Object* idx, Object* val) {
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
  } else {
    fprintf(f, "<unknown>");
  }
}

}  // namespace internal
}  // namespace rapid
