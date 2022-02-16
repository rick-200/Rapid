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
注意：Heap的Alloc函数都不会触发GC，若没有可分配内存，返回nullptr
*/
class Heap : public StaticClass {
 public:
  static Heap *Create();
  static void Destory(Heap *heap);

 public:  //以下函数不会调用GC
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
  //所有Global对象创建完毕后，调用此函数允许GC
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