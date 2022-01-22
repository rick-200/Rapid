#include "handle.h"

#include "debug.h"
#include "global.h"
#include "heap.h"
namespace rapid {
namespace internal {

#define G_HC HandleContainer* __hc_ = Global::GetHC();
#define _this __hc_

HandleContainer* HandleContainer::Create() {
  HandleContainer* p =
      (HandleContainer*)malloc(sizeof(HandleContainer));
  VERIFY(p != nullptr);
  p->m_size = 0;
  p->m_top = (LocalSlotBlock*)malloc(sizeof(LocalSlotBlock));
  p->m_top->pre = nullptr;
  p->m_top->slot[p->m_size++] = nullptr;  // == OpenScope
  p->m_g_first = nullptr;
  return p;
}

void HandleContainer::Destory(HandleContainer* hsc) {}

Object** HandleContainer::OpenLocalSlot(Object* val) {
  // ASSERT(val->IsHeapObject());
  if (val == nullptr) {
    DBG_LOG("null\n");
  }
  G_HC;
  if (_this->m_size == 256) {
    LocalSlotBlock* p = (LocalSlotBlock*)malloc(sizeof(LocalSlotBlock));
    p->pre = _this->m_top;
    _this->m_top = p;
    _this->m_size = 0;
  }
  Object** location = &_this->m_top->slot[_this->m_size++];
  *location = val;
  return location;
}

void HandleContainer::OpenLocalScope() { OpenLocalSlot(nullptr); }

void HandleContainer::CloseLocalScope() {
  G_HC;
  while (true) {
    while (_this->m_size > 0 &&
           (_this->m_top)->slot[_this->m_size - 1] != nullptr) {
      --_this->m_size;
    }
    if (_this->m_size > 0) {
      ASSERT((_this->m_top)->slot[_this->m_size - 1] == nullptr);
      --_this->m_size;
      break;
    }
    LocalSlotBlock* pdel = (_this->m_top);
    (_this->m_top) = (_this->m_top)->pre;
    free(pdel);
    _this->m_size = 256;
    continue;
  }
}

void HandleContainer::OpenGlobalSlot(Object* val, Object*** handle_ref) {
  G_HC;
  GlobalSlot* gs = (GlobalSlot*)malloc(sizeof(GlobalSlot));
  gs->obj = val;
  gs->handle_ref = handle_ref;
  gs->nxt = gs->pre = nullptr;
  gs->state = GlobalHandleState::Strong;
  if (_this->m_g_first == nullptr) {
    _this->m_g_first = gs;
  } else {
    _this->m_g_first->nxt = gs;
    gs->pre = _this->m_g_first;
  }
}

void HandleContainer::CloseGlobalSlot(Object** location) {
  G_HC;
  GlobalSlot* gs = reinterpret_cast<GlobalSlot*>(location);
  if (gs == _this->m_g_first) {
    _this->m_g_first = _this->m_g_first->nxt;
  } else {
    ASSERT(gs->pre != nullptr);
    gs->pre->nxt = gs->nxt;
    if (gs->nxt != nullptr) gs->nxt->pre = gs->pre;
  }
  *gs->handle_ref = nullptr;
  free(gs);
}

void HandleContainer::MakeWeak(Object** location,
                               WeakHandleDestoryCallback callback, void* data) {
  GlobalSlot* gs = reinterpret_cast<GlobalSlot*>(location);
  if (gs->state == GlobalHandleState::Strong)
    gs->state = GlobalHandleState::Weak;
  gs->callback = callback;
  gs->userdata = data;
}

bool HandleContainer::IsWeak(Object** location) {
  GlobalSlot* gs = reinterpret_cast<GlobalSlot*>(location);
  return gs->state != GlobalHandleState::Strong;
};

void HandleContainer::TraceRef(GCTracer* gct) {
  G_HC;
  LocalSlotBlock* p = _this->m_top;
  for (size_t i = 0; i < _this->m_size; i++) {
    if (p->slot[i] != nullptr) gct->Trace(p->slot[i]);
  }
  p = p->pre;
  while (p != nullptr) {
    for (size_t i = 0; i < 256; i++) {
      if (p->slot[i] != nullptr) gct->Trace(p->slot[i]);
    }
    p = p->pre;
  }
  GlobalSlot* pg = _this->m_g_first;
  while (pg != nullptr) {
    if (pg->state == GlobalHandleState::Strong) gct->Trace(pg->obj);
    pg = pg->nxt;
  }
}

}  // namespace internal
}  // namespace rapid