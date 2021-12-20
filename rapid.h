#pragma once
#include <cstdint>
namespace rapid {
template <class T>
class Handle {
  T* m_p;

 public:
};
template <class T>
class GlobalHandle {};
#define API_OBJ_DEF(_t) \
  _t() = delete;        \
  _t(const _t&) = delete;
class Object {
  API_OBJ_DEF(Object);
  bool IsInteger();
  bool IsFloat();
  bool IsBool();
  bool IsString();
  bool IsArray();
  bool IsTable();
};
class Integer : public Object {
  API_OBJ_DEF(Integer);
  int64_t Value();
};
class Float : public Object {
  API_OBJ_DEF(Float);
  double Value();
};
class Bool : public Object {
  API_OBJ_DEF(Bool);
  bool Value();
};
class String : public Object {
  API_OBJ_DEF(String);
  const char* Value();
};
class Array : public Object {
  API_OBJ_DEF(Array);
};
class Table : public Object {
  API_OBJ_DEF(Table);
};
}  // namespace rapid