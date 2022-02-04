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
class Table;
class FixedArray;
class FixedTable;
class VarInfo;
class ExternVarInfo;
class ExternVar;
class SharedFunctionData;
class FunctionData;
class InstructionArray;
class Exception;
class NativeObject;
struct ObjectInterface;
class TryCatchTable;
class StackTraceData;
    /*
注意：Heap的Alloc函数都不会触发GC，若没有可分配内存，TODO:返回nullptr
*/
class Heap : public StaticClass {
  // public:
  // static void *RawAlloc(size_t size);
  // static void RawFree(void *p);

 public:
  static Heap *Create();
  static void Destory(Heap *heap);

 public:  //以下函数不会调用GC
  static String *AllocString(const char *cstr, size_t length);
  static Array *AllocArray(size_t reserved = 0);
  static Table *AllocTable(size_t reserved = 4);
  static FixedArray *AllocFixedArray(size_t length);
  static FixedTable *AllocFixedTable(size_t size);
  static InstructionArray *AllocInstructionArray(size_t length);
  // static Object *NullValue();
  static Object *TrueValue();
  static Object *FalseValue();
  static Exception *AllocException(String *type, String *info, Object *data);
  static VarInfo *AllocVarInfo();
  static TryCatchTable *AllocTryCatchTable();
  static ExternVarInfo *AllocExternVarInfo();
  static SharedFunctionData *AllocSharedFunctionData();
  static ExternVar *AllocExternVar();
  static FunctionData *AllocFunctionData();
  static StackTraceData *AllocStackTraceData();
  static NativeObject *AllocNativeObject(void *data,
                                         const ObjectInterface *interface);

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