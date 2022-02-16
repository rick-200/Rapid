#pragma once
namespace rapid {
namespace internal {
class Heap;
struct GlobalData;
class HandleContainer;
class Executer;
class CompilingMemoryZone;
class ExceptionTree;
class Global {
 public:
  static void Init(GlobalData *data = nullptr);
  static void Close();
  static GlobalData *Get();
  static Heap *GetHeap();
  static HandleContainer *GetHC();
  static CompilingMemoryZone *GetCMZ();
  static Executer *GetExecuter();
  static ExceptionTree *GetExceptionTree();
  // static void SetHeap(Heap *h);
};
}  // namespace internal
}  // namespace rapid