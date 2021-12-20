#include "heap.h"

#include <malloc.h>

#include <cstring>
#include <type_traits>

#include "debug.h"
#include "global.h"
#include "handle.h"
#include "object.h"
namespace rapid {
namespace internal {

constexpr uint64_t hash_string(const char* ps, size_t len) {
  uint64_t ret = 14695981039346656037ULL;
  size_t step = (len >> 8) + 1;  //采样<=256个
  for (size_t i = 0; i < len; i += step) {
    ret ^= ps[i];
    ret *= 1099511628211ULL;
  }
  return ret;
}
class HeapImpl : public Heap {
  size_t m_usage;
  uint8_t m_color;
  Object *m_true, *m_false, *m_null;
  struct {
    HeapObject *first, *last;
  } m_objs, m_roots;
  uint64_t m_object_count;

 public:
  void* RawAlloc(size_t size) {
    void* p = malloc(size);
    DBG_LOG("alloc %llu: %p\n", size, p);
    return p;
  }
  void RawFree(void* p) { free(p); }

  //用于为VS的堆分析提供类型参数
  template <class T>
  __declspec(allocator) T* AllocObject(size_t size) {
    static_assert(std::is_base_of_v<Object, T>, "");
    return (T*)RawAlloc(size);
  }
  void Register(HeapObject* obj) {
    ++this->m_object_count;
    obj->m_nextobj = nullptr;
    this->m_objs.last->m_nextobj = obj;
    this->m_objs.last = obj;
  }

  void RegisterRoot(HeapObject* obj) {
    obj->m_nextobj = nullptr;
    this->m_roots.last->m_nextobj = obj;
    this->m_roots.last = obj;
  }

  void FreeObject(HeapObject* obj) {
    /* switch (obj->m_type) {
       case HeapObjectType::Array: break;
       case HeapObjectType::String: break;
       case HeapObjectType::Table: break;
       case HeapObjectType::FuncData: break;
       default: ASSERT(0);
     }*/
    --this->m_object_count;
    RawFree(obj);
  }

  HeapImpl* Create() {
    HeapImpl* h = (HeapImpl*)RawAlloc(sizeof(HeapImpl));
    h->m_color = 0;
    h->m_usage = 0;
    h->m_object_count = 0;

    h->m_objs.first = h->m_objs.last =
        (HeapObject*)AllocObject<HeapObject>(sizeof(HeapObject));
    h->m_objs.last->m_gctag = h->m_color;
    h->m_objs.last->m_nextobj = nullptr;

    h->m_roots.first = h->m_roots.last =
        (HeapObject*)AllocObject<HeapObject>(sizeof(HeapObject));
    h->m_roots.last->m_gctag = h->m_color;
    h->m_roots.last->m_nextobj = nullptr;

    h->m_null = (SpecialValue*)AllocObject<SpecialValue>(sizeof(SpecialValue));
    h->m_true = (SpecialValue*)AllocObject<SpecialValue>(sizeof(SpecialValue));
    h->m_false = (SpecialValue*)AllocObject<SpecialValue>(sizeof(SpecialValue));

    reinterpret_cast<SpecialValue*>(h->m_null)->m_type =
        HeapObjectType::SpecialValue;
    reinterpret_cast<SpecialValue*>(h->m_true)->m_type =
        HeapObjectType::SpecialValue;
    reinterpret_cast<SpecialValue*>(h->m_false)->m_type =
        HeapObjectType::SpecialValue;

    reinterpret_cast<SpecialValue*>(h->m_null)->m_interface =
        &SpecialValue::Interface;
    reinterpret_cast<SpecialValue*>(h->m_true)->m_interface =
        &SpecialValue::Interface;
    reinterpret_cast<SpecialValue*>(h->m_false)->m_interface =
        &SpecialValue::Interface;

    reinterpret_cast<SpecialValue*>(h->m_null)->m_val = SpecialValue::NullVal;
    reinterpret_cast<SpecialValue*>(h->m_true)->m_val = SpecialValue::TrueVal;
    reinterpret_cast<SpecialValue*>(h->m_false)->m_val = SpecialValue::FalseVal;

    return h;
  }

  void Destory(Heap* heap) {
    // TODO
  }

#define ALLOC_HEAPOBJECT(_p, _t)                    \
  static_assert(std::is_same_v<decltype(_p), _t*>,  \
                "ALLOC_HEAPOBJECT type not match"); \
  _p->m_type = HeapObjectType::_t;                  \
  _p->m_interface = &_t::Interface;                 \
  Register(_p);

  String* AllocString(const char* cstr, size_t length) {
    String* s = (String*)AllocObject<String>(sizeof(String) + length + 1);
    s->m_length = length;
    s->m_hash = hash_string(cstr, length);
    memcpy(s->m_data, cstr, length);
    s->m_data[length] = '\0';
    ALLOC_HEAPOBJECT(s, String);
    return s;
  }
  Array* AllocArray() {
    Array* p = (Array*)AllocObject<Array>(sizeof(Array));
    p->m_length = 0;
    p->m_array = AllocFixedArray(0);
    ALLOC_HEAPOBJECT(p, Array);
    return p;
  }
  Table* AllocTable() {
    Table* p = (Table*)AllocObject<Table>(sizeof(Table));
    p->m_table = AllocFixedTable(8);
    ALLOC_HEAPOBJECT(p, Table);
    return p;
  }
  FixedArray* AllocFixedArray(size_t length) {
    FixedArray* p = (FixedArray*)AllocObject<FixedArray>(
        sizeof(FixedArray) + sizeof(Object*) * length);
    p->m_length = length;
    Object* null_v = NullValue();
    for (size_t i = 0; i < length; i++) p->m_data[i] = null_v;
    ALLOC_HEAPOBJECT(p, FixedArray);
    return p;
  }
  InstructionArray* AllocInstructionArray(size_t length) {
    InstructionArray* p = (InstructionArray*)AllocObject<InstructionArray>(
        sizeof(InstructionArray) + sizeof(Cmd) * length);
    p->m_length = length;
    memset(p->m_data, 0, sizeof(Cmd) * length);
    ALLOC_HEAPOBJECT(p, InstructionArray);
    return p;
  }
  FixedTable* AllocFixedTable(size_t size) {
    FixedTable* p = (FixedTable*)AllocObject<FixedTable>(
        sizeof(FixedTable) + sizeof(FixedTable::Node) * size);
    p->m_size = size;
    p->m_used = 0;
    p->m_pfree = p->m_data;
    memset(p->m_data, 0, sizeof(FixedTable::Node) * size);
    ALLOC_HEAPOBJECT(p, FixedTable);
    return p;
  }
  Object* NullValue() { return this->m_null; }
  Object* TrueValue() { return this->m_true; }
  Object* FalseValue() { return this->m_false; }
#define ALLOC_STRUCT_IMPL(_t)               \
  _t* p = (_t*)AllocObject<_t>(sizeof(_t)); \
  memset(p, 0, sizeof(_t));                 \
  ALLOC_HEAPOBJECT(p, _t);                  \
  return p;
  VarData* AllocVarData() { ALLOC_STRUCT_IMPL(VarData); }
  ExternVarData* AllocExternVarData() { ALLOC_STRUCT_IMPL(ExternVarData); }
  SharedFunctionData* AllocSharedFunctionData() {
    ALLOC_STRUCT_IMPL(SharedFunctionData);
  }
  ExternVar* AllocExternVar() { ALLOC_STRUCT_IMPL(ExternVar); }
  uint64_t ObjectCount() { return this->m_object_count; }
  void DoGC() {
    DBG_LOG("begin gc\n");
    this->m_color = (this->m_color + 1) & 1;
    GCTracer gct(this->m_color);
    DBG_LOG("begin trace\n");
    for (HeapObject* p = this->m_roots.first->m_nextobj; p != nullptr;
         p = p->m_nextobj) {
      gct.Trace(p);
      DBG_LOG("trace object: %p\n", p);
    }
    DBG_LOG("trace HandleContainer\n");
    HandleContainer::TraceRef(&gct);
    DBG_LOG("end trace\n");
    HeapObject *p = this->m_objs.first->m_nextobj, *pre = this->m_objs.first;
    DBG_LOG("begin sweep\n");
    while (p != nullptr) {
      if (p->m_gctag != this->m_color) {
        HeapObject* pdel = p;
        ASSERT((p == this->m_objs.last) == (p->m_nextobj == nullptr));
        if (p == this->m_objs.last) {
          this->m_objs.last = pre;
        }
        p = p->m_nextobj;
        pre->m_nextobj = p;

        DBG_LOG("sweep object:%p\n", pdel);
        FreeObject(pdel);
      } else {
        pre = p;
        p = p->m_nextobj;
      }
    }
    DBG_LOG("end sweep\n");
    DBG_LOG("end gc\n");
  }
};
#define CALL_HEAP_IMPL(_f, ...) ((HeapImpl*)Global::GetHeap())->_f(__VA_ARGS__)
void* Heap::RawAlloc(size_t size) { return CALL_HEAP_IMPL(RawAlloc, size); }
void Heap::RawFree(void* p) { return CALL_HEAP_IMPL(RawFree, p); }

Heap* Heap::Create() { return CALL_HEAP_IMPL(Create); }

void Heap::Destory(Heap* heap) { return CALL_HEAP_IMPL(Destory, heap); }

String* Heap::AllocString(const char* cstr, size_t length) {
  return CALL_HEAP_IMPL(AllocString, cstr, length);
}
Array* Heap::AllocArray() { return CALL_HEAP_IMPL(AllocArray); }
Table* Heap::AllocTable() { return CALL_HEAP_IMPL(AllocTable); }
FixedArray* Heap::AllocFixedArray(size_t length) {
  return CALL_HEAP_IMPL(AllocFixedArray, length);
}
FixedTable* Heap::AllocFixedTable(size_t size) {
  return CALL_HEAP_IMPL(AllocFixedTable, size);
}
InstructionArray* Heap::AllocInstructionArray(size_t length) {
  return CALL_HEAP_IMPL(AllocInstructionArray, length);
}
Object* Heap::NullValue() { return CALL_HEAP_IMPL(NullValue); }
Object* Heap::TrueValue() { return CALL_HEAP_IMPL(TrueValue); }
Object* Heap::FalseValue() { return CALL_HEAP_IMPL(FalseValue); }
VarData* Heap::AllocVarData() { return CALL_HEAP_IMPL(AllocVarData); }
ExternVarData* Heap::AllocExternVarData() {
  return CALL_HEAP_IMPL(AllocExternVarData);
}
SharedFunctionData* Heap::AllocSharedFunctionData() {
  return CALL_HEAP_IMPL(AllocSharedFunctionData);
}
ExternVar* Heap::AllocExternVar() { return CALL_HEAP_IMPL(AllocExternVar); }
uint64_t Heap::ObjectCount() { return CALL_HEAP_IMPL(ObjectCount); }
void Heap::DoGC() { return CALL_HEAP_IMPL(DoGC); }

GCTracer::GCTracer(uint8_t color) : m_color(color) {}

void GCTracer::Trace(HeapObject* p) {
  if (p == nullptr) return;
  p->m_gctag = m_color;
  p->m_interface->trace_ref(p, this);
}
void GCTracer::Trace(Object* p) {
  if (p == nullptr) return;
  if (p->IsHeapObject()) Trace(HeapObject::cast(p));
}
}  // namespace internal
}  // namespace rapid