#pragma once
//#include "object.h"
#include "preprocessors.h"
#include "type.h"
namespace rapid {
namespace internal {
class Object;
class HeapObject;
class String;
class Array;
class Dictionary;
class FixedArray;
class FixedDictionary;
class VarInfoArray;
class ExternVarInfoArray;
class ExternVar;
class SharedFunctionData;
class FunctionData;
class InstructionArray;
class Exception;
class NativeObject;
class StackTraceData;
class TableInfo;
class Table;
struct NativeObjectInterface;
    /*
ע�⣺Heap��Alloc���������ᴥ��GC����û�пɷ����ڴ棬����nullptr
*/
class Heap : public StaticClass {
 public:
  static Heap *Create();
  static void Destory(Heap *heap);

 public:  //���º����������GC
  static String *AllocString(const char *cstr, size_t length);
  static Array *AllocArray(size_t reserved = 0);
  static Dictionary *AllocDictionary(size_t reserved = 4);
  static FixedArray *AllocFixedArray(size_t length);
  static FixedDictionary *AllocFixedDictionary(size_t size);
  static InstructionArray *AllocInstructionArray(size_t length);
  static VarInfoArray *AllocVarInfoArray(size_t length);
  static ExternVarInfoArray *AllocExternVarInfoArray(size_t length);
  static SharedFunctionData *AllocSharedFunctionData();
  static ExternVar *AllocExternVar();
  static FunctionData *AllocFunctionData();
  static StackTraceData *AllocStackTraceData();
  // static NativeObject *AllocNativeObject(NativeObjectBase *obj,
  //                                FreeMemoryFunction free_func);
  static TableInfo *AllocTableInfo(FixedArray *prop_list);
  static Table *AllocTable(TableInfo *info);
  static Exception *AllocException();
  static NativeObject *AllocNativeObject(const NativeObjectInterface *interface,
                                  void *data);

 public:
  static uint64_t ObjectCount();
  static void PrintHeap();
  //����Global���󴴽���Ϻ󣬵��ô˺�������GC
  static void EnableGC();

 public:
  static void DoGC();
  static bool NeedGC();
};

class GCTracer {
  uint8_t m_color;

 public:
  GCTracer(uint8_t color);
  void Trace(HeapObject *p);
  void Trace(Object *p);
  template <class... TArgs>
  inline void TraceAll(TArgs... args) {
    (Trace(args), ...);
  }
};

}  // namespace internal
}  // namespace rapid