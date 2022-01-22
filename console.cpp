#include "console.h"

#include "executer.h"
#include "factory.h"
namespace rapid {
namespace internal {
namespace stdmodule {
void print_one(Object* obj, bool first = false) {
  if (obj->IsNull()) {
    printf("null");
  } else if (obj->IsBool()) {
    printf(obj->IsTrue() ? "true" : "false");
  } else if (obj->IsInteger()) {
    printf("%lld", Integer::cast(obj)->value());
  } else if (obj->IsFloat()) {
    printf("%g", Float::cast(obj)->value());
  } else if (obj->IsString()) {
    printf("'%s'", String::cast(obj)->cstr());
  } else if (obj->IsArray()) {
    Object** begin = Array::cast(obj)->begin();
    Object** end = (first || Array::cast(obj)->length() <= 8)
                       ? Array::cast(obj)->end()
                       : begin + 8;
    printf("[");
    for (Object** p = begin; p < end - 1; p++) {
      print_one(*p);
      printf(", ");
    }
    print_one(end[-1]);
    printf(end == Array::cast(obj)->end() ? "]" : ", ...]");
  } else if (obj->IsFixedArray()) {
    ASSERT(0);  // FixedArray不应从脚本中访问
  } else if (obj->IsTable()) {
    Table* tb = Table::cast(obj);
    TableIterator it = tb->get_iterator();
    printf("{");
    int cnt = 0;
    while (!it.is_end()) {
      if (++cnt == 10) {
        printf("'%s': ", it.key()->cstr());
        print_one(it.value());
        printf(", ...");
        break;
      }
      printf("'%s': ", it.key()->cstr());
      print_one(it.value());
      it.next();
      if (it.is_end()) break;
      printf(", ");
    }
    printf("}");
  } else if (obj->IsFixedTable()) {
    ASSERT(0);
  } else if (obj->IsFunctionData()) {
    printf("<funcdata>%p:%s@%p", FunctionData::cast(obj),
           FunctionData::cast(obj)->shared_data->name->cstr(),
           FunctionData::cast(obj)->shared_data);
  } else if (obj->IsSharedFunctionData()) {
    printf("<sharedfuncdata>%s@%p", SharedFunctionData::cast(obj)->name->cstr(),
           SharedFunctionData::cast(obj));
  } else {
    printf("<unknown object>");
  }
}
Object* print(const Parameters& params) {
  HandleScope hs;
  for (size_t i = 0; i < params.count(); i++) {
    print_one(params[i].ptr(), true);
    if (i != params.count() - 1) printf("    ");
  }
  printf("\n");
  return nullptr;
}
Object* console_get_property(Object* obj, String* name, AccessSpecifier spec) {
  return Failure::PropertyNotFound;
}
Object* console_set_property(Object* obj, String* name, Object* val,
                             AccessSpecifier spec) {
  return Failure::PropertyNotFound;
}
Object* console_invoke_memberfunc(Object* obj, String* name,
                                  const Parameters& params) {
  if (String::Equal(name, "print")) {
    return print(params);
  }
  return nullptr;
}
Object* console_invoke_metafunc(Object* obj, MetaFunctionID id,
                                const Parameters& params) {
  return Failure::NotImplemented;
}
void console_trace_ref(Object* obj, GCTracer*) {}

constexpr ObjectInterface console_interface = {
    console_get_property, console_set_property, console_invoke_memberfunc,
    console_invoke_metafunc, console_trace_ref};

Handle<Object> GetConsoleModule() {
  Handle<NativeObject> x =
      Factory::NewNativeObject(nullptr, &console_interface);
  return x;
}
}  // namespace stdmodule
}  // namespace internal
}  // namespace rapid