/*
此文件定义了所有托管对象，都直接或间接派生自Object
*/

#pragma once
#pragma warning(push)
#pragma warning(disable : 4200)  // 0大小数组
#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <numeric>
#include <tuple>
#include <utility>

#include "allocation.h"
#include "heap.h"
#include "preprocessors.h"
#include "type.h"
#include "util.h"
namespace rapid {
namespace internal {
#define ITER_HEAPOBJ_DERIVED_FINAL(V) \
  V(String)                           \
  V(FixedArray)                       \
  V(FixedDictionary)                  \
  V(Array)                            \
  V(Dictionary)                       \
  V(InstructionArray)                 \
  V(VarInfoArray)                     \
  V(ExternVarInfoArray)               \
  V(ExternVar)                        \
  V(SharedFunctionData)               \
  V(FunctionData)                     \
  V(StackTraceData) V(TableInfo) V(Table) V(Exception) V(NativeObject)

typedef intptr_t Address;

#define TEST_PTR_TAG(_p, _tag) \
  ((reinterpret_cast<Address>(_p) & 0b11) == (_tag))
// clang-format off
#define TAG_INT                   0b11
#define TAG_FLOAT                 0b01
#define TAG_SPECIAL_VALUE         0b10
#define TAG_HEAPOBJ               0b00
#define TAG_BOOL                 0b110
#define TRUE_VALUE              0b0110
#define FALSE_VALUE             0b1110
#define TAG_FAILURE              0b010
#define FAILURE_NORMAL_VALUE    0b0010
#define FAILURE_EXCEPTION_VALUE 0b1010
// clang-format on
#define DEF_R_ACCESSOR(_member) \
  auto _member() { return this->m_##_member; }
#define DEF_CAST(_t)                    \
  static inline _t *cast(Object *obj) { \
    ASSERT(obj->Is##_t());              \
    return reinterpret_cast<_t *>(obj); \
  }

//注意必须在定义的最后应用OBJECT_DEF(type)
//否则Interface中找不到当前类的函数
#define OBJECT_DEF(_t)   \
  _t() = delete;         \
  _t(_t &) = delete;     \
  friend class Heap;     \
  friend class HeapImpl; \
  friend class GCTracer;

enum class HeapObjectType : uint8_t {
  UNKNOWN_TYPE,

  String,  //这几种类型可以从脚本访问，排在一起，便于优化switch
  Array,
  Dictionary,
  Exception,
  Table,
  NativeObject,

  FixedArray,
  FixedDictionary,
  InstructionArray,
  NativeFunction,
  RapidObject,
  TryCatchTable,
  TableInfo,

  VarInfoArray,
  ExternVarInfoArray,

  ExternVar,
  SharedFunctionData,
  FunctionData,
  StackTraceData,
};

//所有对象的根对象
class Object {
 public:
  inline bool IsInteger() { return TEST_PTR_TAG(this, TAG_INT); }
  inline bool IsFloat() { return TEST_PTR_TAG(this, TAG_FLOAT); }
  inline bool IsBool() { return TEST_PTR_TAG(this, TAG_BOOL); }
  inline bool IsFailure() { return TEST_PTR_TAG(this, TAG_FAILURE); }
  inline bool IsHeapObject() {
    return this != nullptr && TEST_PTR_TAG(this, TAG_HEAPOBJ);
  }
  inline bool IsNull() { return this == nullptr; }
  inline bool IsTrue() {
    return reinterpret_cast<uintptr_t>(this) == TRUE_VALUE;
  }
  inline bool IsFalse() {
    return reinterpret_cast<uintptr_t>(this) == FALSE_VALUE;
  }
#define DEF_ISXXX(_t) inline bool Is##_t();
  ITER_HEAPOBJ_DERIVED_FINAL(DEF_ISXXX);
#undef DEF_ISXXX

 public:
  OBJECT_DEF(Object);
};
#define value_true reinterpret_cast<Object *>(TRUE_VALUE)
#define value_false reinterpret_cast<Object *>(FALSE_VALUE)
//表示函数执行失败，原因不重要或根据函数约定（如需要gc，函数未实现等）
//若需要获取执行失败的原因，不应增加failure的数量，应利用引用等获取具体原因
#define value_failure_normal reinterpret_cast<Object *>(FAILURE_NORMAL_VALUE)
//表示函数执行失败，原因是抛出了异常
#define value_failure_exception \
  reinterpret_cast<Object *>(FAILURE_EXCEPTION_VALUE)
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
  static Float *NaN() {
    static constexpr double v = std::numeric_limits<double>::quiet_NaN();
    uint64_t v64 = *(uint64_t *)&v;
    ASSERT(v64 == (v64 & (~TAG_FLOAT)));
    v64 = v64 | TAG_FLOAT;
    return *(Float **)&v64;
  }
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

class String;
class FunctionData;
class HeapObjectTag {
  uint64_t m_data;
  static constexpr uint64_t TYPE_MASK = 0xFF;
  static constexpr uint64_t SIZE_MASK = 0xFFFF'FFFF'FF00;
  static constexpr uint64_t SIZE_OFFSET = 8;
  static constexpr uint64_t GCCOLOR_MASK = 0x10000'0000'0000;
  static constexpr uint64_t GCCOLOR_OFFSET = 48;

 public:
  HeapObjectTag(HeapObjectType type, uint64_t allocated_size)
      : m_data(static_cast<uint64_t>(type) | (allocated_size << 8)) {
    ASSERT(allocated_size < (1ULL << 48));
  }
  HeapObjectType type() { return static_cast<HeapObjectType>(m_data & 0xFF); }
  uint64_t allocated_size() { return (m_data & SIZE_MASK) >> SIZE_OFFSET; }
  uint8_t gccolor() {
    return static_cast<uint8_t>((m_data & GCCOLOR_MASK) >> GCCOLOR_OFFSET);
  }
  void set_gccolor(uint8_t c) {
    ASSERT(c == 1 || c == 0);
    m_data = (m_data & (~GCCOLOR_MASK)) | ((uint64_t)c << GCCOLOR_OFFSET);
  }
};
constexpr int xxxx = sizeof(HeapObjectTag);
//所有分配在堆上的对象的基类
class HeapObject : public Object {
 private:
  friend class Object;
  HeapObjectTag m_tag;
  HeapObject *m_nextobj;

 public:
  void trace_member(GCTracer *gct);
  HeapObjectType type() { return m_tag.type(); }

 public:
  DEF_CAST(HeapObject)
  OBJECT_DEF(HeapObject);
};

//字符串对象
class String : public HeapObject {
  size_t m_length;
  uint64_t m_hash;
  bool m_cached;
  char m_data[];

 public:
  void trace_member(GCTracer *gct) {}

 public:
  char *cstr() { return m_data; }
  size_t length() { return m_length; }
  uint64_t hash() { return m_hash; }

  static bool Equal(String *s1, String *s2) {
    if (s1 == s2) return true;
    if (s1->m_cached && s2->m_cached) return false;
    if (s1->m_hash != s2->m_hash || s1->m_length != s2->m_length) return false;
    size_t len = s1->m_length;
    for (size_t i = 0; i < len; i++) {
      if (s1->m_data[i] != s2->m_data[i]) return false;
    }
    return true;
  }
  static bool Equal(String *s1, const CStringWrap &s2) {
    if (s1->m_hash != s2.hash || s1->m_length != s2.length) return false;
    size_t len = s1->m_length;
    for (size_t i = 0; i < len; i++) {
      if (s1->m_data[i] != s2.s[i]) return false;
    }
    return true;
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
  void trace_member(GCTracer *gct) {
    for (size_t i = 0; i < m_length; i++) gct->Trace(m_data[i]);
  }

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
  OBJECT_DEF(FixedArray)
  DEF_CAST(FixedArray)
};
// template <class T>
// class TypedFixedArrayHandle {
//  FixedArray *m_array;
//
// public:
//  TypedFixedArrayHandle(FixedArray *array) : m_array(array) {}
//  FixedArray *array() { return m_array; }
//  T *get() { return T::cast(m_array->get(pos)); }
//  void set(size_t pos, T *val) { m_array->set(pos, val); }
//  T *operator[](size_t pos) { return T::cast(m_array->get(pos)); }
//  T **begin() { return reinterpret_cast<T **>(m_array->begin()); }
//  T **end() { return reinterpret_cast<T **>(m_array->end()); }
//};

struct DictionaryNode {
  String *key;
  Object *val;
  DictionaryNode *next;
};
class DictionaryIterator {
  const DictionaryNode *m_p, *m_end;

 private:
  friend class FixedDictionary;

 private:
  DictionaryIterator(const DictionaryNode *begin, const DictionaryNode *end)
      : m_p(begin), m_end(end) {
    if (m_p->key == nullptr) next();
  }

 public:
  DictionaryIterator(const DictionaryIterator &) = default;
  bool is_end() { return m_p == m_end; }
  void next() {
    if (!is_end()) {
      do {
        ++m_p;
      } while (m_p->key == nullptr);
    }
  }
  String *key() { return m_p->key; }
  Object *value() { return m_p->val; }
};

//长度固定的String* -> Object*哈希表
class FixedDictionary : public HeapObject {
 private:
  size_t m_size;
  size_t m_used;
  DictionaryNode *m_pfree;
  DictionaryNode m_data[];

 public:
  void trace_member(GCTracer *gct) {
    for (size_t i = 0; i < m_size; i++) {
      if (m_data[i].key != nullptr) {
        gct->Trace(m_data[i].key);
        gct->Trace(m_data[i].val);
      }
    }
  }

 private:
  bool has_free() {
    DictionaryNode *pend = m_data + m_size;
    while (m_pfree < pend && m_pfree->key != nullptr) ++m_pfree;
    return m_pfree < pend;
  }
  //调用前必须先调用has_free
  DictionaryNode *new_node() {
    ASSERT(m_pfree < m_data + m_size && m_pfree->key == nullptr);
    ++m_used;
    return m_pfree++;
  }
  size_t mainpos(String *key) { return key->hash() % m_size; }
  DictionaryNode *find_node(String *key) {
    DictionaryNode *p = m_data + mainpos(key);
    if (p->key == nullptr) return nullptr;
    while (p != nullptr) {
      ASSERT(mainpos(p->key) == mainpos(key));
      if (String::Equal(p->key, key)) return p;
      p = p->next;
    }
    return nullptr;
  }
  DictionaryNode *find_or_create_node(String *key) {
    DictionaryNode *p = m_data + mainpos(key);
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
      if (String::Equal(p->key, key)) return p;
      if (!has_free()) return nullptr;
      p->next = new_node();
      p->next->key = key;
      return p->next;
    }
    //存在哈希冲突
    if (!has_free()) return nullptr;  //一定需要新建节点
    DictionaryNode *trans =
        new_node();  //准备将mainpos(key)节点移动到新节点，以保证一条链上的mainpos一致
    DictionaryNode *ppre = m_data + mainpos(p->key);
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
    DictionaryNode *p = find_node(key);
    if (p == nullptr) return nullptr;
    Object *ret = p->val;
    while (p->next != nullptr) {
      DictionaryNode *pnxt = p->next;
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
    DictionaryNode *p = find_node(key);
    if (p == nullptr) return value_failure_normal;
    return p->val;
  }
  Object *set(String *key, Object *val, bool allow_new = true) {
    DictionaryNode *p;
    if (allow_new) {
      p = find_or_create_node(key);
      if (p == nullptr) return value_failure_normal;
    } else {
      p = find_node(key);
      if (p == nullptr) return value_failure_normal;
    }
    Object *old = p->val;
    p->val = val;
    return old;
  }
  //注意remove()删除的节点不一定会被重新利用
  Object *remove(String *key) {
    Object *obj = remove_node(key);
    if (obj == nullptr) return value_failure_normal;
    return obj;
  }
  bool exists(String *key) { return find_node(key) != nullptr; }
  size_t size() { return m_size; }
  // 注意因为remove()不更新pfree，used为当前有值的节点数，并非实际占用的节点数
  // used()<size()时仍可能无法新建节点
  size_t used() { return m_used; }

  void rehash_to(FixedDictionary *other) {
    ASSERT(m_used <= other->m_size);
    ASSERT(other->m_used == 0);
    for (DictionaryNode *p = m_data; p < m_data + m_size; p++) {
      if (p->key != nullptr) other->find_or_create_node(p->key)->val = p->val;
    }
  }
  DictionaryIterator get_iterator() {
    return DictionaryIterator(m_data, m_data + m_size);
  }

 public:
  OBJECT_DEF(FixedDictionary)
  DEF_CAST(FixedDictionary)
};

//长度可变的Object*数组
class Array : public HeapObject {
  FixedArray *m_array;
  size_t m_length;

 public:
  void trace_member(GCTracer *gct) { gct->Trace(m_array); }

 private:
  void change_capacity(size_t new_cap);
  void try_shrink() {
    if (m_array->length() > (m_length << 2)) change_capacity(m_length << 1);
  }

 public:
  size_t length() { return m_length; }
  size_t capacity() { return m_array->length(); }
  Object *get(size_t pos) {
    if (pos >= m_length) return value_failure_normal;
    return m_array->get(pos);
  }
  Object *set(size_t pos, Object *val) {
    if (pos >= m_length) return value_failure_normal;
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
      // Object *null_v = Heap::NullValue();
      for (size_t i = m_length; i < new_len; i++) m_array->set(i, nullptr);
      m_length = new_len;
    } else if (new_len < m_length) {
      m_length = new_len;
      try_shrink();
    }
  }
  Object **begin() { return m_array->begin(); }
  Object **end() { return m_array->begin() + m_length; }
  void quick_init(Object **params, size_t count) {
    ASSERT(m_length == 0);
    ASSERT(capacity() >= count);
    m_length = count;
    for (size_t i = 0; i < count; i++) {
      m_array->set(i, params[i]);
    }
  }

 public:
  OBJECT_DEF(Array)
  DEF_CAST(Array)
};

//长度可变的String* -> Object*哈希表
class Dictionary : public HeapObject {
  FixedDictionary *m_table;

 public:
  void trace_member(GCTracer *gct) { return gct->Trace(m_table); }

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

  //仅用于MAKE_TABLE指令创建表时填入初始数据
  void quick_init(Object **params, size_t count) {
    ASSERT(m_table->used() == 0);
    ASSERT(count * 2 <= m_table->size());
    while (count--) {
      Object *x = m_table->set(String::cast(params[0]), params[1], true);
      ASSERT(!x->IsFailure());
      params += 2;
    }
  }

  Object *remove(String *key) {
    Object *obj = m_table->remove(key);
    try_rehash_shrink();
    return obj;
  }
  bool exists(String *key) { return m_table->exists(key); }
  static void trace_ref(Object *obj, GCTracer *gct) {
    Dictionary *_this = Dictionary::cast(obj);
    gct->Trace(_this->m_table);
  }
  DictionaryIterator get_iterator() { return m_table->get_iterator(); }

 public:
  OBJECT_DEF(Dictionary)
  DEF_CAST(Dictionary)
};
class InstructionArray : public HeapObject {
  size_t m_length;
  byte m_data[];

 public:
  void trace_member(GCTracer *gct) {}

 public:
  OBJECT_DEF(InstructionArray)
  DEF_CAST(InstructionArray)
  size_t length() { return m_length; }
  byte *begin() { return m_data; }
};
struct VarInfoStruct {
  String *name;
  uint16_t slot_id;
};
class VarInfoArray : public HeapObject {
  size_t m_length;
  VarInfoStruct m_data[];

 public:
  void trace_member(GCTracer *gct) {
    for (size_t i = 0; i < m_length; i++) gct->Trace(m_data[i].name);
  }
  size_t length() { return m_length; }
  VarInfoStruct &at(size_t pos) {
    ASSERT(pos < m_length);
    return m_data[pos];
  }
  DEF_CAST(VarInfoArray)
  OBJECT_DEF(VarInfoArray)
};
struct ExternVarInfoStruct {
  String *name;
  bool
      in_stack;  //变量是否存在于外层函数的栈中（否则存在于外层函数的extern_var中）
  uint16_t pos;  //所在栈或extern_var的位置
};
class ExternVarInfoArray : public HeapObject {
  size_t m_length;
  ExternVarInfoStruct m_data[];

 public:
  void trace_member(GCTracer *gct) {
    for (size_t i = 0; i < m_length; i++) gct->Trace(m_data[i].name);
  }
  size_t length() { return m_length; }
  ExternVarInfoStruct &at(size_t pos) {
    ASSERT(pos < m_length);
    return m_data[pos];
  }
  DEF_CAST(ExternVarInfoArray)
  OBJECT_DEF(ExternVarInfoArray)
};

class ExternVar : public HeapObject {
 public:
  //外部变量的引用，应通过value_ref访问和修改变量
  //若value_ref不指向自身value，则其必指向栈slot
  Object **value_ref;
  union {
    Object *value;  //当所在函数退出时，用于保存变量的值，不应通过value访问变量
    ExternVar *
        next;  //当所在函数未退出时，所有属于此函数栈的ExternVar构成一个链表，用于查询
  } un;

 public:
  void trace_member(GCTracer *gct) {
    gct->Trace(*value_ref);
    if (is_open()) {
      gct->Trace(un.next);
    }
  }

 public:
  OBJECT_DEF(ExternVar)
  DEF_CAST(ExternVar)
  bool is_open() { return value_ref != &un.value; }
};

class SharedFunctionData : public HeapObject {
 public:
  String *name;
  String *filename;  //所在文件名
  InstructionArray *instructions;
  FixedArray *kpool;
  FixedArray *inner_func;
  VarInfoArray *vars;
  ExternVarInfoArray *extvars;
  FixedArray *bytecode_line;
  FixedArray *tableinfo;
  size_t max_stack;
  size_t param_cnt;
 public:
  void trace_member(GCTracer *gct) {
    gct->TraceAll(name, filename, instructions, kpool, inner_func, vars,
                  extvars, bytecode_line, tableinfo);
  }

 public:
  OBJECT_DEF(SharedFunctionData)
  DEF_CAST(SharedFunctionData)
};
class FunctionData : public HeapObject {
 public:
  SharedFunctionData *shared_data;
  FixedArray *extvars;
  ExternVar *open_extvar_head;

 public:
  void trace_member(GCTracer *gct) {
    gct->TraceAll(shared_data, extvars, open_extvar_head);
  }

 public:
  OBJECT_DEF(FunctionData)
  DEF_CAST(FunctionData)
};
class StackTraceData : public HeapObject {
 public:
  bool is_script;
  uint32_t line;
  String *filename;
  String *funcname;

 public:
  void trace_member(GCTracer *gct) { gct->TraceAll(filename, funcname); }

 public:
  OBJECT_DEF(StackTraceData)
  DEF_CAST(StackTraceData)
};
// table的描述信息
class TableInfo : public HeapObject {
  FixedDictionary *m_prop_idx;
  FixedArray *m_prop_name;

 public:
  static constexpr size_t invalid_index = ~0ULL;
 public:
  size_t prop_count() { return m_prop_name->length(); }
  size_t get_index(String *name) {
    Object *res = m_prop_idx->get(name);
    if (res == value_failure_normal) {
      return invalid_index;
    }
    return Integer::cast(res)->value();
  }
  String *get_name(size_t idx) { return String::cast(m_prop_name->get(idx)); }

 public:
  void trace_member(GCTracer *gct) { gct->TraceAll(m_prop_idx, m_prop_name); }

 public:
  DEF_CAST(TableInfo)
  OBJECT_DEF(TableInfo)
};

class Table : public HeapObject {
  TableInfo *m_info;
  Object *m_data[];

 public:
  TableInfo *table_info() { return m_info; }
  Object *get(size_t idx) {
    ASSERT(idx < m_info->prop_count());
    return m_data[idx];
  }
  void set(size_t idx, Object *value) {
    ASSERT(idx < m_info->prop_count());
    m_data[idx] = value;
  }

 public:
  void trace_member(GCTracer *gct) {
    gct->Trace(m_info);
    for (size_t i = 0; i < m_info->prop_count(); i++) gct->Trace(m_data[i]);
  }

 public:
  OBJECT_DEF(Table)
  DEF_CAST(Table)
};

class Exception : public HeapObject {
 public:
  uint64_t id;
  String *info;
  String *stack_trace;

 public:
  void trace_member(GCTracer *gct) { gct->TraceAll(info, stack_trace); }

 public:
  OBJECT_DEF(Exception)
  DEF_CAST(Exception)
};
struct NativeObjectInterface;
class NativeObject : public HeapObject {
  const NativeObjectInterface *m_interface;
  void *m_data;

 public:
  void trace_member(GCTracer *gct) {}
  // void trace_member(GCTracer *gct) {
  //  for (size_t i = 0; i < m_obj->m_ref_array_count; i++)
  //    gct->Trace(*m_obj->m_slot_array[i]);
  //}

 public:
  const NativeObjectInterface *interface() { return m_interface; }
  void *data() { return m_data; }

 public:
  OBJECT_DEF(NativeObject)
  DEF_CAST(NativeObject)
};

void debug_print(FILE *f, Object *obj);

// ---------------Object Inline Implement-----------------

inline void HeapObject::trace_member(GCTracer *gct) {
  switch (m_tag.type()) {
#define CASE_T(_t)                                                \
  case HeapObjectType::_t:                                        \
    static_assert(&_t::trace_member != &HeapObject::trace_member, \
                  "未实现trace_member");                          \
    _t::cast(this)->trace_member(gct);                            \
    break;
    ITER_HEAPOBJ_DERIVED_FINAL(CASE_T)
#undef CASE_T
    default:
      ASSERT(0);
  }
}

#define IS_HEAPOBJECT(_t) \
  (IsHeapObject() && HeapObject::cast(this)->m_tag.type() == HeapObjectType::_t)

#define IMPL_IS_HEAPOBJ_DERIVED_FINAL(_t) \
  bool Object::Is##_t() { return IS_HEAPOBJECT(_t); }

ITER_HEAPOBJ_DERIVED_FINAL(IMPL_IS_HEAPOBJ_DERIVED_FINAL)
#undef IMPL_IS_HEAPOBJ_DERIVED_FINAL
#undef IS_HEAPOBJECT

// Object *Object::GetProperty(String *name, AccessSpecifier spec) {
//  if (!this->IsHeapObject()) return Failure::PropertyNotFound;
//  return HeapObject::cast(this)->get_interface()->get_property(this, name,
//                                                               spec);
//}
// Object *Object::SetProperty(String *name, Object *val, AccessSpecifier spec)
// {
//  if (!this->IsHeapObject()) return Failure::PropertyNotFound;
//  return HeapObject::cast(this)->get_interface()->set_property(this, name,
//  val,
//                                                               spec);
//}
// inline Object *Object::InvokeMemberFunc(String *name,
//                                        const Parameters &params) {
//  if (this->IsHeapObject()) {
//    return HeapObject::cast(this)->get_interface()->invoke_memberfunc(
//        this, name, params);
//  } else {
//    VERIFY(0);  // TODO:实现值类型的InvokeMemberFunc
//  }
//  return nullptr;
//}
// inline Object *Object::InvokeMetaFunc(MetaFunctionID id,
//                                      const Parameters &params) {
//  if (this->IsHeapObject()) {
//    return HeapObject::cast(this)->get_interface()->invoke_metafunc(this, id,
//                                                                    params);
//  } else {
//    VERIFY(0);  // TODO:实现值类型的InvokeMetaFunc
//  }
//  return nullptr;
//}
//
// Object *Object::GetMemberFunc(String *name, AccessSpecifier spec) {
//  if (this->IsHeapObject()) {
//    return HeapObject::cast(this)->get_interface()->get_memberfunc(this, name,
//                                                                   spec);
//  } else {
//    VERIFY(0);
//  }
//}
//--------------------------------------------------------------------

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
// class HashDictionary : public Object {
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
//  OBJECT_DEF(HashDictionary);
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
/*
class Animal{
  name;
  age;
  private uid;
  $init(name,age){
    this.name=name;
    this.age=age;
    this.id=generate_uid();
  }
  protected move(){
    print("animal move");
  }
}
class Dog: Animal{
  name;
  age;
  private uid;
  $init(name,age): super(name,age){

  }
  walk(){
    print("dog walk");
    super.move();
  }
}





*/
/*
TODO:大改类型系统

- Object
  - Integer
  - Float
  - Bool
  - HeapObject
    - TypeInfo           //用于为RSObject提供类型信息
    - String
    - FixedArray
    - Array
    - FixedDictionary
    - Dictionary
    - SharedFunctionData
    - Struct
      - ... (利用模板的奇技淫巧简洁地定义各种struct)
    - RSObject
//能被脚本代码直接访问的对象，脚本通过Type查询对象的类型信息从而访问对象的成员

HeapObjectTag:(64位) |tttttttt|ssssssssssssssssssssssssssssssssssssssss|...|
                      type[8]      size[40]                           gctag[1]
reserved[] HeapObjectTag为64位整数，作为HeapObject的第一个成员
使用HeapObjectTag标识堆对象的类型等信息

func A(){
  var func_table = {
    func_a:()=>{
      return this.a;
    },
    func_b:()=>{
      return this.b;
    }
  };
  func _new_A(){
    var obj = {
      $super:func_table,
      a:1,
      b:2,
    }
    return obj;
  }
  return _new_a;
}


*/
