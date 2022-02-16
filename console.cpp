#include "standerd_module.h"

#include "executer.h"
#include "factory.h"
#include "native_object_interface.h"
#include "uuid.h"
namespace rapid {
namespace internal {
namespace stdmodule {
static void print_one(Object* obj, bool first = false) {
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
  } else if (obj->IsDictionary()) {
    Dictionary* tb = Dictionary::cast(obj);
    DictionaryIterator it = tb->get_iterator();
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
  } else if (obj->IsFixedDictionary()) {
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
static Object* print(const Parameters& params) {
  for (size_t i = 0; i < params.count(); i++) {
    print_one(params[i], true);
    if (i != params.count() - 1) printf("    ");
  }
  printf("\n");
  return nullptr;
}
class Console {
 public:
  static constexpr UUID uuid = {0x17EF779BA88E032, 0x15902275E1A53FF7};
  static Object* get_property(void* data, String* name) {
    HandleScope hs;
    if (String::Equal(name, CStringWrap("print"))) {
      return *NewNativeFunction(print);
    } else {
      return value_failure_normal;
    }
  }
  static constexpr NativeObjectInterface interface = {
      uuid, get_property, nullptr, nullptr, nullptr,
  };
};
Handle<Object> GetConsoleModule() {
  return Factory::NewNativeObject(&Console::interface, nullptr);
}
}  // namespace stdmodule
}  // namespace internal
}  // namespace rapid