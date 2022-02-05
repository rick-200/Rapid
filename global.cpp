#include "global.h"

#include "ast.h"
#include "executer.h"
#include "handle.h"
#include "heap.h"
#include "preprocessors.h"
#include <malloc.h>
namespace rapid {
namespace internal {
struct GlobalData {
  size_t ref_cnt;
  Heap *heap;
  HandleContainer *hsc;
  CompilingMemoryZone *cmz;
  Executer *exec;
};
static thread_local GlobalData *global_data;
void Global::Init(GlobalData *data) {
  ASSERT(global_data == nullptr);
  if (data != nullptr) {
    global_data = data;
    return;
  }
  global_data = Allocate<GlobalData>();
  VERIFY(global_data != nullptr);
  global_data->ref_cnt = 1;
  global_data->heap = Heap::Create();
  global_data->hsc = HandleContainer::Create();
  HandleScope hs;
  global_data->cmz = CompilingMemoryZone::Create();
  global_data->exec = Executer::Create();
  Heap::EnableGC();
}

void Global::Close() {
  ASSERT(global_data != nullptr);
  if (--global_data->ref_cnt == 0) {
    // TODO£ºÊµÏÖGlobal::Close
  }
}

GlobalData *Global::Get() { return global_data; }

Heap *Global::GetHeap() { return global_data->heap; }

HandleContainer *Global::GetHC() { return global_data->hsc; }

CompilingMemoryZone *Global::GetCMZ() { return global_data->cmz; }

Executer *Global::GetExecuter() { return global_data->exec; }

// void Global::SetHeap(Heap* h) { global_data->heap = h; }

} // namespace internal
} // namespace rapid