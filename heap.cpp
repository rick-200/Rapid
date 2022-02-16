#include "heap.h"

#include <malloc.h>

#include <cstring>
#include <type_traits>

#include "debug.h"
#include "exception_tree.h"
#include "executer.h"
#include "global.h"
#include "handle.h"
#include "object.h"
#include "util.h"
namespace rapid {
namespace internal {

class HeapImpl /*public: Heap --
                  不应该继承，否则Heap中调用函数时可能调用未被重写的函数*/
{
  size_t m_usage;
  size_t m_last_gc_usage;  //上次gc后
  size_t m_memory_limit;   //内存使用的上限
  uint8_t m_color;
  Object *m_true, *m_false;
  bool m_enable_gc;
  // Object  *m_null;
  IntrusiveForwardList<HeapObject, &HeapObject::m_nextobj> m_objs;

 public:
  //用于为VS的堆分析提供类型参数
  template <class T>
  __declspec(allocator) T *AllocObject(size_t size) {
    static_assert(std::is_base_of_v<HeapObject, T>, "");
    if (m_usage + size > m_memory_limit) return nullptr;
    T *p = (T *)malloc(size);
    if (p == nullptr) return nullptr;
    m_usage += size;
    return p;
  }
  void Register(HeapObject *obj) {
    obj->m_tag.set_gccolor(this->m_color);
    m_objs.push(obj);
  }

  // void RegisterRoot(HeapObject *obj) {
  //  obj->m_nextobj = nullptr;
  //  this->m_roots.last->m_nextobj = obj;
  //  this->m_roots.last = obj;
  //}

  void FreeObject(HeapObject *obj) {
    m_usage -= obj->m_tag.allocated_size();
    // if (obj->IsNativeObject()) {
    //  NativeObject::cast(obj)->m_free(NativeObject::cast(obj)->m_obj);
    //}
    free(obj);
  }

  static HeapImpl *Create() {
    HeapImpl *h = Allocate<HeapImpl>();
    VERIFY(h != nullptr);
    h->m_color = 0;
    h->m_usage = 0;
    h->m_enable_gc = false;
    h->m_last_gc_usage = 1024;
    Construct(&h->m_objs);
    return h;
  }

  static void Destory(Heap *heap) {
    // TODO:实现HeapImpl::Destory
  }

  String *AllocString(const char *cstr, size_t length) {
    size_t alloc_size = sizeof(String) + length + 1;
    String *s = (String *)AllocObject<String>(alloc_size);
    if (s == nullptr) return nullptr;
    s->m_tag = HeapObjectTag{HeapObjectType::String, alloc_size};
    s->m_length = length;
    s->m_hash = hash_string(cstr, length);
    s->m_cached = false;
    memcpy(s->m_data, cstr, length);
    s->m_data[length] = '\0';
    Register(s);
    return s;
  }
  Array *AllocArray(size_t reserved) {
    Array *p = (Array *)AllocObject<Array>(sizeof(Array));
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::Array, sizeof(Array)};
    p->m_length = 0;
    p->m_array = AllocFixedArray(reserved);
    if (p->m_array == nullptr) {
      free(p);
      return nullptr;
    }
    Register(p);
    return p;
  }
  Dictionary *AllocDictionary(size_t reserve) {
    Dictionary *p = (Dictionary *)AllocObject<Dictionary>(sizeof(Dictionary));
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::Dictionary, sizeof(Dictionary)};
    p->m_table = AllocFixedDictionary(reserve * 2);
    if (p->m_table == nullptr) {
      free(p);
      return nullptr;
    }
    Register(p);
    return p;
  }
  FixedArray *AllocFixedArray(size_t length) {
    size_t alloc_size = sizeof(FixedArray) + sizeof(Object *) * length;
    FixedArray *p = (FixedArray *)AllocObject<FixedArray>(alloc_size);
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::FixedArray, alloc_size};
    p->m_length = length;
    for (size_t i = 0; i < length; i++) p->m_data[i] = nullptr;
    Register(p);
    return p;
  }
  InstructionArray *AllocInstructionArray(size_t length) {
    size_t alloc_size = sizeof(InstructionArray) + sizeof(byte) * length;
    InstructionArray *p =
        (InstructionArray *)AllocObject<InstructionArray>(alloc_size);
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::InstructionArray, alloc_size};
    p->m_length = length;
    memset(p->m_data, 0, sizeof(byte) * length);
    Register(p);
    return p;
  }
  FixedDictionary *AllocFixedDictionary(size_t size) {
    size_t alloc_size = sizeof(FixedDictionary) + sizeof(DictionaryNode) * size;
    FixedDictionary *p =
        (FixedDictionary *)AllocObject<FixedDictionary>(alloc_size);
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::FixedDictionary, alloc_size};
    p->m_size = size;
    p->m_used = 0;
    p->m_pfree = p->m_data;
    memset(p->m_data, 0, sizeof(DictionaryNode) * size);
    Register(p);
    return p;
  }
  // NativeObject *AllocNativeObject(NativeObjectBase *obj,
  //                                FreeMemoryFunction free_func) {
  //  NativeObject *p = AllocObject<NativeObject>(sizeof(NativeObject));
  //  if (p == nullptr) return nullptr;
  //  p->m_tag =
  //      HeapObjectTag{HeapObjectType::NativeObject, sizeof(NativeObject)};
  //  p->m_obj = obj;
  //  p->m_free = free_func;
  //  Register(p);
  //  return p;
  //}

  VarInfoArray *AllocVarInfoArray(size_t length) {
    size_t alloc_size = sizeof(VarInfoArray) + sizeof(VarInfoStruct) * length;
    VarInfoArray *p = AllocObject<VarInfoArray>(alloc_size);
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::VarInfoArray, alloc_size};
    p->m_length = length;
    Register(p);
    return p;
  }
  ExternVarInfoArray *AllocExternVarInfoArray(size_t length) {
    size_t alloc_size =
        sizeof(ExternVarInfoArray) + sizeof(ExternVarInfoStruct) * length;
    ExternVarInfoArray *p = AllocObject<ExternVarInfoArray>(alloc_size);
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::ExternVarInfoArray, alloc_size};
    p->m_length = length;
    Register(p);
    return p;
  }
  TableInfo *AllocTableInfo(FixedArray *prop_list) {
    FixedDictionary *prop_dict = AllocFixedDictionary(prop_list->length() * 2);
    for (size_t i = 0; i < prop_list->length(); i++) {
      Object *ret = prop_dict->set(String::cast(prop_list->get(i)),
                                   Integer::FromInt64(i));
      ASSERT(!ret->IsFailure());
    }
    if (prop_dict == nullptr) return nullptr;
    TableInfo *p = AllocObject<TableInfo>(sizeof(TableInfo));
    if (p == nullptr) {
      free(prop_dict);
      return nullptr;
    }
    p->m_tag = HeapObjectTag{HeapObjectType::TableInfo, sizeof(TableInfo)};
    p->m_prop_idx = prop_dict;
    p->m_prop_name = prop_list;
    Register(p);
    return p;
  }
  Table *AllocTable(TableInfo *info) {
    size_t alloc_size = sizeof(Table) + sizeof(Object *) * info->prop_count();
    Table *p = AllocObject<Table>(alloc_size);
    if (p == nullptr) return nullptr;
    p->m_tag = HeapObjectTag{HeapObjectType::Table, alloc_size};
    p->m_info = info;
    memset(p->m_data, 0, sizeof(Object *) * info->prop_count());
    Register(p);
    return p;
  }
  NativeObject *AllocNativeObject(const NativeObjectInterface *interface,
                                  void *data) {
    NativeObject *p = AllocObject<NativeObject>(sizeof(NativeObject));
    if (p == nullptr) return nullptr;
    p->m_tag =
        HeapObjectTag{HeapObjectType::NativeObject, sizeof(NativeObject)};
    p->m_interface = interface;
    p->m_data = data;
    return p;
  }

  template <class T>
  T *AllocStruct(HeapObjectType t) {
    T *p = (T *)AllocObject<T>(sizeof(T));
    if (p == nullptr) return nullptr;
    memset(p, 0, sizeof(T));
    p->m_tag = HeapObjectTag{t, sizeof(T)};
    Register(p);
    return p;
  }
#define ALLOC_STRUCT(_T) AllocStruct<_T>(HeapObjectType::_T)
  FunctionData *AllocFunctionData() { return ALLOC_STRUCT(FunctionData); }
  ExternVar *AllocExternVar() { return ALLOC_STRUCT(ExternVar); }
  SharedFunctionData *AllocSharedFunctionData() {
    return ALLOC_STRUCT(SharedFunctionData);
  }
  StackTraceData *AllocStackTraceData() { return ALLOC_STRUCT(StackTraceData); }
  Exception *AllocException() { return ALLOC_STRUCT(Exception); }
#undef ALLOC_STRUCT

  void EnableGC() { m_enable_gc = true; }
  uint64_t ObjectCount() { return m_objs.count(); }
  void DoGC() {
    if (!m_enable_gc) return;
    size_t pre_gc_usage = m_usage;
    // return;  // NO_GC
    // DBG_LOG("begin gc\n");
    this->m_color = (this->m_color + 1) & 1;
    GCTracer gct(this->m_color);
    Executer::TraceStack(&gct);
    HandleContainer::TraceRef(&gct);
    ExceptionTree::TraceMember(&gct);
    m_objs.for_each([this](HeapObject *p) -> bool {
      if (p->m_tag.gccolor() == m_color) return false;
      DBG_LOG("sweep object:%p\n", p);
      FreeObject(p);
      return true;
    });
    DBG_LOG("gc complete: before:%llu after:%llu reduce:%lld\n", pre_gc_usage,
            m_usage, pre_gc_usage - m_usage);
    m_last_gc_usage = m_usage;
  }
  void DoGCIfNeed() {
    if (m_usage > m_last_gc_usage * 2) DoGC();
  }
  bool NeedGC() { return m_usage > m_last_gc_usage * 2; }
  void PrintHeap() {
    m_objs.for_each([](HeapObject *p) -> bool {
      debug_print(stdout, p);
      printf("\n");
      return false;
    });
  }
};
#define CALL_HEAP_IMPL(_f, ...) \
  (reinterpret_cast<HeapImpl *>(Global::GetHeap())->_f(__VA_ARGS__))

Heap *Heap::Create() { return reinterpret_cast<Heap *>(HeapImpl::Create()); }

void Heap::Destory(Heap *heap) { return HeapImpl::Destory(heap); }

String *Heap::AllocString(const char *cstr, size_t length) {
  return CALL_HEAP_IMPL(AllocString, cstr, length);
}
Array *Heap::AllocArray(size_t reserved) {
  return CALL_HEAP_IMPL(AllocArray, reserved);
}
Dictionary *Heap::AllocDictionary(size_t reserved) {
  return CALL_HEAP_IMPL(AllocDictionary, reserved);
}
FixedArray *Heap::AllocFixedArray(size_t length) {
  return CALL_HEAP_IMPL(AllocFixedArray, length);
}
FixedDictionary *Heap::AllocFixedDictionary(size_t size) {
  return CALL_HEAP_IMPL(AllocFixedDictionary, size);
}
InstructionArray *Heap::AllocInstructionArray(size_t length) {
  return CALL_HEAP_IMPL(AllocInstructionArray, length);
}
VarInfoArray *Heap::AllocVarInfoArray(size_t length) {
  return CALL_HEAP_IMPL(AllocVarInfoArray, length);
}
ExternVarInfoArray *Heap::AllocExternVarInfoArray(size_t length) {
  return CALL_HEAP_IMPL(AllocExternVarInfoArray, length);
}
SharedFunctionData *Heap::AllocSharedFunctionData() {
  return CALL_HEAP_IMPL(AllocSharedFunctionData);
}
ExternVar *Heap::AllocExternVar() { return CALL_HEAP_IMPL(AllocExternVar); }
FunctionData *Heap::AllocFunctionData() {
  return CALL_HEAP_IMPL(AllocFunctionData);
}
StackTraceData *Heap::AllocStackTraceData() {
  return CALL_HEAP_IMPL(AllocStackTraceData);
}
Exception *Heap::AllocException() { return CALL_HEAP_IMPL(AllocException); }
// NativeObject *Heap::AllocNativeObject(NativeObjectBase *obj,
//                                      FreeMemoryFunction free_func) {
//  return CALL_HEAP_IMPL(AllocNativeObject, obj, free_func);
//}

TableInfo *Heap::AllocTableInfo(FixedArray *prop_list) {
  return CALL_HEAP_IMPL(AllocTableInfo, prop_list);
}
Table *Heap::AllocTable(TableInfo *info) {
  return CALL_HEAP_IMPL(AllocTable, info);
}
NativeObject *Heap::AllocNativeObject(const NativeObjectInterface *interface,
                                      void *data) {
  return CALL_HEAP_IMPL(AllocNativeObject, interface, data);
}
uint64_t Heap::ObjectCount() { return CALL_HEAP_IMPL(ObjectCount); }
void Heap::PrintHeap() { return CALL_HEAP_IMPL(PrintHeap); }
void Heap::EnableGC() { return CALL_HEAP_IMPL(EnableGC); }
void Heap::DoGC() { return CALL_HEAP_IMPL(DoGC); }
bool Heap::NeedGC() { return CALL_HEAP_IMPL(NeedGC); }

GCTracer::GCTracer(uint8_t color) : m_color(color) {}

void GCTracer::Trace(HeapObject *p) {
  if (p == nullptr) return;
  if (p->m_tag.gccolor() == m_color) return;
  p->m_tag.set_gccolor(m_color);
  p->trace_member(this);
}
void GCTracer::Trace(Object *p) {
  if (p == nullptr) return;
  if (p->IsHeapObject()) Trace(HeapObject::cast(p));
}
}  // namespace internal
}  // namespace rapid