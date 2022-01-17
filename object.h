/*
此文件定义了所有托管对象，都直接或间接派生自Object
*/

#pragma once
#pragma warning(push)
#pragma warning(disable : 4200)  // 0大小数组

#include <utility>

#include "heap.h"
#include "preprocessors.h"
#include "type.h"
namespace rapid {
namespace internal {
/*
- Object
  - Integer
  - Float
  - Failure
  - HeapObject
    - SpecialValue
    - String
    - FixedArray
    - Array
    - FixedTable
    - Table
    - FixedByteArray
    - ByteArray
    - FunctionData
    - Exception
    - Struct
      - VarData
      - ExternVarData
*/
#define ITER_HEAPOBJ_DERIVED_FINAL(V) \
  V(SpecialValue)                     \
  V(String)                           \
  V(FixedArray)                       \
  V(FixedTable)                       \
  V(Array)                            \
  V(Table)                            \
  V(FixedByteArray)                   \
  V(ByteArray)                        \
  V(InstructionArray)                 \
  V(Exception)                        \
  V(NativeFunction)                   \
  V(FunctionData)                     \
  V(VarData)                          \
  V(ExternVarData)                    \
  V(ExternVar)                        \
  V(SharedFunctionData)

#define ITER_STRUCT_DERIVED(V) \
  V(VarData)                   \
  V(ExternVarData) V(ExternVar) V(SharedFunctionData) V(FunctionData)

typedef intptr_t Address;

#define TEST_PTR_TAG(_p, _tag) \
  ((reinterpret_cast<Address>(_p) & 0b11) == (_tag))
#define TAG_INT 0b11
#define TAG_FLOAT 0b01
#define TAG_FAILURE 0b10
#define TAG_HEAPOBJ 0b00
#define DEF_R_ACCESSOR(_member) \
  auto _member() { return this->m_##_member; }
#define DEF_CAST(_t)                    \
  static inline _t *cast(Object *obj) { \
    ASSERT(obj->Is##_t());              \
    return reinterpret_cast<_t *>(obj); \
  }

//注意必须在定义的最后应用OBJECT_DEF(type)
//否则Interface中找不到当前类的函数
#define OBJECT_DEF(_t)                           \
  _t() = delete;                                 \
  _t(_t &) = delete;                             \
  friend class Heap;                             \
  friend class HeapImpl;                         \
  friend class GCTracer;                         \
  static constexpr ObjectInterface Interface = { \
      get_member, set_member, get_index, set_index, trace_ref};

enum class HeapObjectType {
  UNKNOWN_TYPE,
  String,
  FixedArray,
  FixedByteArray,
  FixedTable,
  Array,
  ByteArray,
  Table,
  SpecialValue,
  InstructionArray,
  Exception,
  NativeFunction,
  Flag_Struct_Start,
  Struct,
  VarData,
  ExternVarData,
  ExternVar,
  SharedFunctionData,
  FunctionData,
  Flag_Struct_End,
};

//所有对象的根对象
class Object {
 public:
  inline bool IsInteger() { return TEST_PTR_TAG(this, TAG_INT); }
  inline bool IsFloat() { return TEST_PTR_TAG(this, TAG_FLOAT); }
  inline bool IsFailure() { return TEST_PTR_TAG(this, TAG_FAILURE); }
  inline bool IsHeapObject() { return TEST_PTR_TAG(this, TAG_HEAPOBJ); }
#define DEF_ISXXX(_t) inline bool Is##_t();
  ITER_HEAPOBJ_DERIVED_FINAL(DEF_ISXXX);

  DEF_ISXXX(True);
  DEF_ISXXX(False);
  DEF_ISXXX(Bool);
  DEF_ISXXX(Null);
  DEF_ISXXX(Struct);

 protected:
  static Object *get_member(Object *obj, String *name);
  static Object *set_member(Object *obj, String *name, Object *val);
  static Object *get_index(Object *obj, Object *idx);
  static Object *set_index(Object *obj, Object *idx, Object *val);
  static void trace_ref(Object *obj, GCTracer *gct);

 public:
  OBJECT_DEF(Object);
};
// 62位整数，保存于指针内
class Integer : public Object {
 public:
  int64_t value() { return reinterpret_cast<int64_t>(this) >> 2; }
  static constexpr int64_t max_value = 0X1FFF'FFFF'FFFF'FFFFLL;
  static constexpr int64_t min_value = 0XE000'0000'0000'0000LL;
  static Integer *FromInt64(int64_t v) {
    ASSERT(v >= min_value && v <= max_value);
    return reinterpret_cast<Integer *>((v << 2) | TAG_INT);
  }
  DEF_CAST(Integer)
  OBJECT_DEF(Integer)
};
// 62位浮点数，由double牺牲两位尾数而来
//保存于指针内
class Float : public Object {
 public:
  double value() {
    uint64_t v = reinterpret_cast<Address>(this) ^ TAG_FLOAT;
    return *reinterpret_cast<double *>(&v);
  }
  static Float *FromDouble(double v) {
    return reinterpret_cast<Float *>(
        (*reinterpret_cast<uint64_t *>(&v) & 0xFFFF'FFFF'FFFF'FFFC) |
        TAG_FLOAT);
  }
  DEF_CAST(Float)
  OBJECT_DEF(Float)
};
//表示失败的操作，内含一个预定义的错误代码
class Failure : public Object {
 public:
#define FAILURE_VAL(code) \
  reinterpret_cast<Failure *>(((code) << 2) | TAG_FAILURE)
  static constexpr Failure *Exception = FAILURE_VAL(1);
  static constexpr Failure *OutOfMemory = FAILURE_VAL(2);
  static constexpr Failure *RetryAfterGC = FAILURE_VAL(3);
  static constexpr Failure *NoAuthority = FAILURE_VAL(4);
  static constexpr Failure *MemberNotFound = FAILURE_VAL(5);
  static constexpr Failure *IndexNotFound = FAILURE_VAL(5);
  static constexpr Failure *NoEnoughSpace = FAILURE_VAL(5);
  static constexpr Failure *NotImplemented = FAILURE_VAL(6);
  static constexpr Failure *Expection = FAILURE_VAL(7);  //运行时错误
  uint32_t code() { return (uint32_t)(reinterpret_cast<Address>(this) >> 2); }
  DEF_CAST(Failure)
  OBJECT_DEF(Failure);
};

class String;
class FunctionData;

//所有分配在堆上的对象的基类
class HeapObject : public Object {
 private:
  friend class Object;
  HeapObjectType m_heapobj_type;
  uint8_t m_gctag;
  HeapObject *m_nextobj;

 protected:
  const ObjectInterface *m_interface;

 public:
 public:
  DEF_CAST(HeapObject)
  OBJECT_DEF(HeapObject);
};

//用于保存特殊的值：null, true, false
class SpecialValue : public HeapObject {
  // 0:null 1:true 2:false
  uint8_t m_val;

 private:
  static constexpr uint8_t NullVal = 0;
  static constexpr uint8_t TrueVal = 1;
  static constexpr uint8_t FalseVal = 2;

 public:
  OBJECT_DEF(SpecialValue);
};

//包裹一个本地函数指针
class NativeFunction : public HeapObject {
 public:
  NativeFunctionPtr func;
  void *data;
  NativeFuncObjGCCallback gc_callback;

 public:
  Object *call(const Parameters &params) { return func(data, params); }
  void on_gc() { gc_callback(func, data); }

 public:
  OBJECT_DEF(NativeFunction);
  DEF_CAST(NativeFunction);
};

//包裹一个本地对象
// TODO
class NativeObject : public HeapObject {
  void *m_p;
  void *m_data;
  void *m_gc_callback;
  ObjectInterface *custom_interface;

 public:
};

//字符串对象
class String : public HeapObject {
  size_t m_length;
  uint64_t m_hash;
  char m_data[];

 public:
  char *cstr() { return m_data; }
  size_t length() { return m_length; }
  uint64_t hash() { return m_hash; }

  static bool Equal(String *s1, String *s2) {
    if (s1 == s2) return true;
    if (s1->m_hash != s2->m_hash || s1->m_length != s2->m_length) return false;
    size_t len = s1->m_length;
    for (size_t i = 0; i < len; i++) {
      if (s1->m_data[i] != s2->m_data[i]) return false;
    }
    return true;
  }

 private:
  static Object *get_member(Object *obj, String *name) {
    return Failure::Exception;
  }

 public:
  OBJECT_DEF(String);
  DEF_CAST(String);
};

//长度固定的Object*数组
class FixedArray : public HeapObject {
  size_t m_length;
  Object *m_data[];

 public:
  size_t length() { return m_length; }
  Object *get(size_t pos) {
    ASSERT(pos < m_length);
    return m_data[pos];
  }
  Object *set(size_t pos, Object *val) {
    ASSERT(pos < m_length);
    Object *old = m_data[pos];
    m_data[pos] = val;
    return old;
  }
  Object **begin() { return m_data; }
  Object **end() { return m_data + m_length; }

 public:
  static void trace_ref(Object *obj, GCTracer *gct) {
    FixedArray *_this = FixedArray::cast(obj);
    for (size_t i = 0; i < _this->m_length; i++) {
      gct->Trace(_this->m_data[i]);
    }
  }

 public:
  OBJECT_DEF(FixedArray)
  DEF_CAST(FixedArray)
};

//长度固定的String* -> Object*哈希表
class FixedTable : public HeapObject {
  struct Node {
    String *key;
    Object *val;
    Node *next;
  };

 private:
  size_t m_size;
  size_t m_used;
  Node *m_pfree;
  Node m_data[];

 private:
  bool has_free() {
    Node *pend = m_data + m_size;
    while (m_pfree < pend && m_pfree->key != nullptr) ++m_pfree;
    return m_pfree < pend;
  }
  //调用前必须先调用has_free
  Node *new_node() {
    ASSERT(m_pfree < m_data + m_size && m_pfree->key == nullptr);
    ++m_used;
    return m_pfree++;
  }
  size_t mainpos(String *key) { return key->hash() % m_size; }
  Node *find_node(String *key) {
    Node *p = m_data + mainpos(key);
    if (p->key == nullptr) return nullptr;
    while (p != nullptr) {
      ASSERT(mainpos(p->key) == mainpos(key));
      if (String::Equal(p->key, key)) return p;
      p = p->next;
    }
    return nullptr;
  }
  Node *find_or_create_node(String *key) {
    Node *p = m_data + mainpos(key);
    if (p->key == nullptr) {
      p->key = key;
      ++m_used;
      return p;
    }
    if (mainpos(p->key) == mainpos(key)) {  //无哈希冲突
      while (p->next != nullptr) {
        ASSERT(mainpos(p->key) == mainpos(key));
        if (String::Equal(p->key, key)) return p;
        p = p->next;
      }
      if (!has_free()) return nullptr;
      p->next = new_node();
      p->next->key = key;
      return p->next;
    }
    //存在哈希冲突
    if (!has_free()) return nullptr;  //一定需要新建节点
    Node *trans =
        new_node();  //准备将mainpos(key)节点移动到新节点，以保证一条链上的mainpos一致
    Node *ppre = m_data + mainpos(p->key);
    while (ppre->next != p) ppre = ppre->next;
    *trans = *p;
    ppre->next = trans;
    p->key = key;
    p->val = nullptr;
    p->next = nullptr;
    return p;
  }
  // 不会更新pfree，即删除的节点不一定会被重新利用
  // 若更新pfree，会导致极端情况下插入复杂度退化至O(n)
  // 特殊情况：若删除的节点正好位于mainpos且next==nullptr且pfree指向此节点之前，则一定会被重新利用
  //          若删除的节点正好位于mainpos且next==nullptr且pfree指向此节点之后，则插入此mainpos时会被重新利用
  Object *remove_node(String *key) {
    Node *p = find_node(key);
    if (p == nullptr) return nullptr;
    Object *ret = p->val;
    while (p->next != nullptr) {
      Node *pnxt = p->next;
      *p = *pnxt;
      p = pnxt;
    }
    //空出节点p
    p->key = nullptr;
    p->val = nullptr;
    p->next = nullptr;
    --m_used;
    return ret;
  }

 public:
  Object *get(String *key) {
    Node *p = find_node(key);
    if (p == nullptr) return Failure::IndexNotFound;
    return p->val;
  }
  Object *set(String *key, Object *val, bool allow_new = true) {
    Node *p;
    if (allow_new) {
      p = find_or_create_node(key);
      if (p == nullptr) return Failure::NoEnoughSpace;
    } else {
      p = find_node(key);
      if (p == nullptr) return Failure::IndexNotFound;
    }
    Object *old = p->val;
    p->val = val;
    return old;
  }
  //注意remove()删除的节点不一定会被重新利用
  Object *remove(String *key) {
    Object *obj = remove_node(key);
    if (obj == nullptr) return Failure::IndexNotFound;
    return obj;
  }
  bool exists(String *key) { return find_node(key) != nullptr; }
  size_t size() { return m_size; }
  // 注意因为remove()不更新pfree，used为当前有值的节点数，并非实际占用的节点数
  // used()<size()时仍可能无法新建节点
  size_t used() { return m_used; }

  void rehash_to(FixedTable *other) {
    ASSERT(m_used <= other->m_size);
    ASSERT(other->m_used == 0);
    for (Node *p = m_data; p < m_data + m_size; p++) {
      if (p->key != nullptr) other->find_or_create_node(p->key)->val = p->val;
    }
  }

 public:
  static void trace_ref(Object *obj, GCTracer *gct) {
    FixedTable *_this = FixedTable::cast(obj);
    for (size_t i = 0; i < _this->m_size; i++) {
      if (_this->m_data[i].key != nullptr) {
        gct->Trace(_this->m_data[i].key);
        gct->Trace(_this->m_data[i].val);
      }
    }
  }

 public:
  OBJECT_DEF(FixedTable)
  DEF_CAST(FixedTable)
};
//用于分配一块连续的，由GC管理的内存
class FixedByteArray : public HeapObject {
  size_t m_length;
  byte m_data[];

 public:
  size_t length() { return m_length; }
  byte *data() { return m_data; }

 public:
  OBJECT_DEF(FixedByteArray)
  DEF_CAST(FixedByteArray)
};

//用于分配一块连续的，动态增长的，由GC管理的内存
//注意：与Array不同，ByteArray不会自动收缩
class ByteArray : public HeapObject {
  size_t m_length;
  FixedByteArray *m_array;

 private:
  void change_capacity(size_t new_cap) {}
  static inline size_t apply_alignment(size_t size, size_t alignment) {
    return size - size % alignment + alignment;
  }

 public:
  size_t length() { return m_length; }
  size_t capacity() { return m_array->length(); }
  byte *data() { return m_array->data(); }
  void reserve(size_t new_cap, size_t alignment) {
    ASSERT(new_cap % alignment == 0);
    if (new_cap <= capacity()) return;
    change_capacity(
        std::max(new_cap, apply_alignment((capacity() << 1) | 1, alignment)));
  }
  void resize(size_t new_size, size_t alignment) {
    reserve(new_size, alignment);
    m_length = new_size;
  }
  void shrink_to_fit() { change_capacity(m_length); }

 public:
  OBJECT_DEF(ByteArray)
  DEF_CAST(ByteArray)
};

template <class T>
class FixedByteArrayView {
  FixedByteArray *m_array;

 public:
};

//提供操作ByteArray的接口
//将ByteArray当作类似std::vector<T>使用
template <class T>
class ByteArrayView {
  ByteArray *m_array;
#define BYTE_ARRAY_VIEW_CHECK                 \
  ASSERT(m_array->length() % sizeof(T) == 0); \
  ASSERT(m_array->capacity() % sizeof(T) == 0)
 private:
  void inner_reserve(size_t size) {
    m_array->reserve(size * sizeof(T), sizeof(T));
  }
  void inner_resize(size_t size) {
    m_array->resize(size * sizeof(T), sizeof(T));
  }

 public:
  T *begin() {
    BYTE_ARRAY_VIEW_CHECK;
    return reinterpret_cast<T *>(m_array->data());
  }
  T *end() {
    BYTE_ARRAY_VIEW_CHECK;
    return reinterpret_cast<T *>(m_array->data() + m_array->length());
  }
  T get(size_t pos) {
    ASSERT(pos < length());
    return *(begin() + pos);
  }
  void set(size_t pos, const T &val) {
    ASSERT(pos < length());
    *(begin() + pos) = val;
  }
  T &operator[](size_t pos) {
    ASSERT(pos < length());
    return *(begin() + pos);
  }
  void push(const T &val) {
    inner_resize(length() + 1);
    set(length() - 1, val);
  }
  void pop() { inner_resize(length() - 1); }
  void shrink_to_fit() { m_array->shrink_to_fit(); }
  void reserve(size_t size) { inner_reserve(size); }
  void resize(size_t size, const T &val = T{}) {
    size_t old_end = length();
    inner_resize(size);
    for (size_t i = old_end; i < length(); i++) set(i, val);
  }
  size_t length() {
    BYTE_ARRAY_VIEW_CHECK;
    return m_array->length() / sizeof(T);
  }
};
//长度可变的Object*数组
class Array : public HeapObject {
  FixedArray *m_array;
  size_t m_length;

 private:
  void change_capacity(size_t new_cap);
  void try_shrink() {
    if (m_array->length() > (m_length << 2)) change_capacity(m_length << 1);
  }

 public:
  size_t length() { return m_length; }
  size_t capacity() { return m_array->length(); }
  Object *get(size_t pos) {
    if (pos >= m_length) return Failure::IndexNotFound;
    return m_array->get(pos);
  }
  Object *set(size_t pos, Object *val) {
    if (pos >= m_length) return Failure::IndexNotFound;
    return m_array->set(pos, val);
  }
  void push(Object *val) {
    if (m_length == m_array->length())
      change_capacity((m_array->length() << 1) | 1);  //每次增加一倍，至少增加1
    m_array->set(m_length++, val);
  }
  void reserve(size_t new_cap) {
    if (new_cap > m_array->length())  //至少增加一倍
      change_capacity(std::max(new_cap, (m_array->length() << 1) | 1));
  }
  void pop() {
    --m_length;
    try_shrink();
  }
  void shrink_to_fit() { change_capacity(m_length); }
  void resize(size_t new_len) {
    if (new_len > m_length) {
      reserve(new_len);
      Object *null_v = Heap::NullValue();
      for (size_t i = m_length; i < new_len; i++) m_array->set(i, null_v);
      m_length = new_len;
    } else if (new_len < m_length) {
      m_length = new_len;
      try_shrink();
    }
  }

 private:
  static Object *get_index(Object *obj, Object *idx) {
    // TODO
    return Failure::Exception;
  }
  static void trace_ref(Object *obj, GCTracer *gct) {
    Array *_this = Array::cast(obj);
    FixedArray::trace_ref(_this->m_array, gct);
  }

 public:
  OBJECT_DEF(Array)
  DEF_CAST(Array)
};

//长度可变的String* -> Object*哈希表
class Table : public HeapObject {
  FixedTable *m_table;

 private:
  void rehash_expand();
  void try_rehash_shrink();

 public:
  Object *get(String *key) { return m_table->get(key); }
  Object *set(String *key, Object *val, bool allow_new = true) {
    if (!allow_new) return m_table->set(key, val, false);

    if (m_table->used() * 2 > m_table->size()) rehash_expand();
    Object *x = m_table->set(key, val, true);
    ASSERT(!x->IsFailure());
    return x;
  }
  Object *remove(String *key) {
    Object *obj = m_table->remove(key);
    try_rehash_shrink();
    return obj;
  }
  bool exists(String *key) { return m_table->exists(key); }
  static void trace_ref(Object *obj, GCTracer *gct) {
    Table *_this = Table::cast(obj);
    FixedTable::trace_ref(_this->m_table, gct);
  }

 public:
  OBJECT_DEF(Table)
  DEF_CAST(Table)
};
class InstructionArray : public HeapObject {
  size_t m_length;
  Cmd m_data[];

 public:
  OBJECT_DEF(InstructionArray)
  DEF_CAST(InstructionArray)
  size_t length() { return m_length; }
  Cmd *begin() { return m_data; }
};

class Exception : public HeapObject {
  String *m_type;
  String *m_info;
  Object *m_data;

 private:
  static void trace_ref(Object *obj, GCTracer *gct) {
    Exception *_this = Exception::cast(obj);
    gct->Trace(_this->m_type);
    gct->Trace(_this->m_info);
    gct->Trace(_this->m_data);
  }

 public:
  OBJECT_DEF(Exception)
  DEF_CAST(Exception)
  DEF_R_ACCESSOR(type);
  DEF_R_ACCESSOR(info);
  DEF_R_ACCESSOR(data);
};

//包含简单数据的对象的基类
class Struct : public HeapObject {
 public:
  OBJECT_DEF(Struct)
  DEF_CAST(Struct)
};

//用于实现Struct派生类的trace_ref
#define DECL_TRACEREF(_t, ...)                        \
  static void trace_ref(Object *obj, GCTracer *gct) { \
    _t *_this = _t::cast(obj);                        \
    gct->TraceAll(__VA_ARGS__);                       \
  }
#define _M(_m) _this->_m

class VarData : public Struct {
 public:
  String *name;

 private:
  DECL_TRACEREF(VarData, _M(name));

 public:
  OBJECT_DEF(VarData)
  DEF_CAST(VarData)
};
class ExternVarData : public Struct {
 public:
  String *name;
  bool in_stack;
  uint8_t pos;

 private:
  DECL_TRACEREF(ExternVarData, _M(name));

 public:
  OBJECT_DEF(ExternVarData)
  DEF_CAST(ExternVarData)
};

class ExternVar : public Struct {
 public:
  Object *value;
  bool in_stack;
  uint8_t pos;

 private:
  DECL_TRACEREF(ExternVar, _M(value));

 public:
  OBJECT_DEF(ExternVar)
  DEF_CAST(ExternVar)
};

class SharedFunctionData : public Struct {
 public:
  String *name;
  InstructionArray *instructions;
  FixedArray *kpool;
  FixedArray *inner_func;
  FixedArray *vars;
  FixedArray *extvars;
  size_t max_stack;
  size_t param_cnt;

 private:
  DECL_TRACEREF(SharedFunctionData, _M(name), _M(instructions), _M(kpool),
                _M(inner_func), _M(vars), _M(extvars));

 public:
  OBJECT_DEF(SharedFunctionData)
  DEF_CAST(SharedFunctionData)
};
class FunctionData : public Struct {
 public:
  SharedFunctionData *shared_data;
  FixedArray *extvars;

 private:
  DECL_TRACEREF(FunctionData, _M(shared_data), _M(extvars));

 public:
  OBJECT_DEF(FunctionData)
  DEF_CAST(FunctionData)
};
#undef DECL_TRACEREF
#undef _M

// ----Object Implement--------
#define IS_HEAPOBJECT(_t) \
  (IsHeapObject() &&      \
   HeapObject::cast(this)->m_heapobj_type == HeapObjectType::_t)
#define IMPL_IS_HEAPOBJ_DERIVED_FINAL(_t) \
  bool Object::Is##_t() { return IS_HEAPOBJECT(_t); }
ITER_HEAPOBJ_DERIVED_FINAL(IMPL_IS_HEAPOBJ_DERIVED_FINAL)
bool Object::IsNull() { return this == Heap::NullValue(); }
bool Object::IsTrue() { return this == Heap::TrueValue(); }
bool Object::IsFalse() { return this == Heap::FalseValue(); }
bool Object::IsBool() { return IsTrue() || IsFalse(); }
bool Object::IsStruct() {
  return IsHeapObject() &&
         HeapObject::cast(this)->m_heapobj_type >
             HeapObjectType::Flag_Struct_Start &&
         HeapObject::cast(this)->m_heapobj_type <
             HeapObjectType::Flag_Struct_End;
}
#undef IMPL_IS_HEAPOBJ_DERIVED_FINAL
#undef IS_HEAPOBJECT
//--------------------------

//
// class RSFunction : public RSObject {
//  OBJECT_DEF(RSFunction);
//};
// class Array : public HeapObject {
// protected:
//  uint64_t m_len, m_cap;
//
// public:
//  size_t length() { return m_len; }
//  size_t capacity() { return m_cap; }
//  OBJECT_DEF(Array);
//};
// class FixedArray : public Array {
//  Object* m_data[];
//
// public:
//  Object* get(size_t pos) {
//    ASSERT(pos < m_len);
//    return m_data[pos];
//  }
//  Object* set(size_t pos, Object* val) {
//    ASSERT(pos < m_len);
//    m_data[pos] = val;
//  }
//  OBJECT_DEF(FixedArray);
//};
// class DynamicArray : public Array {
//  Object* m_data;
//
// public:
//  OBJECT_DEF(DynamicArray);
//};
//
// class HashTable : public Object {
// public:
//  struct Node {
//    String* key;
//    Object* val;
//    Node* next;
//  };
//
// private:
//  size_t m_size;
//  size_t m_freepos;
//  Node m_data[];
//
// public:
//  Object* get(String* name) {
//    Node* p = &m_data[name->hash() % m_size];
//    while (p != nullptr) {
//      if (p->key == name) return p->val;
//      p = p->next;
//    }
//  }
//  void set(String* name, Object* val) {
//    Node* p = &m_data[name->hash() % m_size];
//    if (p->key == nullptr) {
//      p->key = name;
//      p->val = val;
//      return;
//    }
//    Node* pre = p;
//    p = p->next;
//    while (p != nullptr) {
//      pre = p;
//      p = p->next;
//    }
//    while (m_data[m_freepos].key != nullptr) {
//      ++m_freepos;
//      ASSERT(m_freepos < m_size);
//    }
//    pre->next = &m_data[m_freepos];
//    m_data[m_freepos].key = name;
//    m_data[m_freepos].val = val;
//  }
//  size_t size() { return m_size; }
//  Node* data() { return m_data; }
//  OBJECT_DEF(HashTable);
//};
//
//
// class SpecialValue : public HeapObject {
// public:
//  OBJECT_DEF(SpecialValue);
//};

}  // namespace internal
}  // namespace rapid
#pragma warning(pop)
