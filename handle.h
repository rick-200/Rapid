#pragma once
#include "global.h"
#include "object.h"
namespace rapid {
namespace internal {
class GCTracer;
struct LocalSlotBlock {
  Object* slot[256];
  LocalSlotBlock* pre;
};
enum class GlobalHandleState {
  Strong,    //此引用为强引用
  Weak,      //此引用为弱引用
  WeakOnly,  //此对象仅有弱引用
};
typedef void (*WeakHandleDestoryCallback)(Object* obj, void* data);
struct GlobalSlot {
  Object* obj;
  GlobalSlot *pre, *nxt;
  GlobalHandleState state;
  Object*** handle_ref;
  WeakHandleDestoryCallback callback;
  void* userdata;
};
class HandleContainer {
  LocalSlotBlock* m_top;
  size_t m_size;
  GlobalSlot* m_g_first;

 public:
  // HandleSlotContainer();
  static HandleContainer* Create();
  static void Destory(HandleContainer* hsc);
  static Object** OpenLocalSlot(Object* val);
  static void OpenLocalScope();
  static void CloseLocalScope();

  static void OpenGlobalSlot(Object* val, Object*** handle_ref);
  static void CloseGlobalSlot(Object** location);
  static void MakeWeak(Object** location, WeakHandleDestoryCallback callback,
                       void* data);       //仅对GlobalHandle使用
  static bool IsWeak(Object** location);  //仅对GlobalHandle使用

  static void TraceRef(GCTracer* gct);
};
class HandleScope {
 public:
  HandleScope() { Global::GetHC()->OpenLocalScope(); };
  ~HandleScope() { Global::GetHC()->CloseLocalScope(); };
};
// template <class T>
// class Handle {
//  T* m_p;
//
// public:
//  Handle() : m_p(nullptr) {}
//  explicit Handle(T* p) : m_p(Global::GetHC()->OpenLocalSlot(p)) {}
//  template <class TOther>
//  Handle(const Handle<TOther>& other) : m_p(other.m_p) {}
//  T* ptr() { return m_p; }
//  T* operator->() { return m_p; }
//  bool empty() { return m_p == nullptr; }
//};

// template <class T>
// class Handle {
//  T** m_location;
//
// public:
//  Handle() { m_location = nullptr; }
//  explicit Handle(T* p) { m_location =
//  reinterpret_cast<T**>(Global::GetHC()->OpenLocalSlot(p)); } explicit
//  Handle(T** p) { m_location = p; } template <class TOther> Handle(const
//  Handle<TOther>& other) {
//    T* check1;
//    TOther* check2;
//    check1 = check2;
//    m_location = reinterpret_cast<T**>(other.location());
//  }
//  T* ptr() const { return *m_location; }
//  T* operator->() { return *m_location; }
//  T** location() { return m_location; }
//  bool empty() { return m_location == nullptr; }
//};


struct non_local_t {};
//构造Handle<T>时使用，表示对象已经做好GC保护，不再开辟LocalSlot
constexpr non_local_t non_local;

//对对象指针的简单封装
//Handle包裹的指针保证在此Handle生命周期内不会被GC回收
//默认使用HandleScope进行管理
template <class T>
class Handle {
  T* m_p;

 public:
  Handle() = default;
  Handle(const Handle& h) = default;
  //Handle& operator=(const Handle& h) = default;
  explicit Handle(T* p) {
    if (p != nullptr)
      m_p = reinterpret_cast<T*>(*Global::GetHC()->OpenLocalSlot(p));
    else
      m_p = nullptr;
  }

  //不使用HandleScope管理，必须保证对象在此Handle生命周期内不会被GC回收
  explicit Handle(T* p, const non_local_t&) : m_p(p) {}

  template <class TOther,
            std::enable_if_t<std::is_convertible_v<TOther*, T*>, int> = 0>
  Handle(const Handle<TOther>& other) : m_p(other.ptr()) {
    ASSERT(m_p == other.ptr());
  }

  T* ptr() const { return m_p; }
  T* operator->() const { return m_p; }
  T* operator*() { return m_p; }

  bool empty() const { return m_p == nullptr; }

  template <class TOther>
  static Handle<T> cast(const Handle<TOther>& other) {
    Handle<T> ret;
    ret.m_p = T::cast(other.ptr());
    return ret;
  }
};


template <class T>
class GlobalHandle {
  T** m_p;

 public:
  GlobalHandle() : m_p(nullptr) {}
  ~GlobalHandle() { Dispose(); }
  explicit GlobalHandle(T* p) {
    Global::GetHC()->OpenGlobalSlot(p, reinterpret_cast<Object***>(&m_p));
    ASSERT(m_p != nullptr);
  }

  template <class TOther,
            std::enable_if_t<std::is_convertible_v<TOther*, T*>, int> = 0>
  GlobalHandle(const Handle<TOther>& h) : GlobalHandle(h.ptr()) {}

  template <class TOther>
  GlobalHandle(const GlobalHandle<TOther>& other) : GlobalHandle(other.ptr()) {}

  template <class TOther,
            std::enable_if_t<std::is_convertible_v<TOther*, T*>, int> = 0>
  GlobalHandle(GlobalHandle<TOther>&& other) : m_p(other.detach()) {}

  GlobalHandle& operator=(const GlobalHandle&) = delete;

  template <class TOther,
            std::enable_if_t<std::is_convertible_v<TOther*, T*>, int> = 0>
  GlobalHandle& operator=(GlobalHandle<TOther>&& other) {
    ASSERT(empty());
    m_p = other.detach();
  }

  T* ptr() const { return *m_p; }
  T* operator->() const { return *m_p; }
  T** detach() {
    T** ret = m_p;
    m_p = nullptr;
    return ret;
  }
  bool empty() const { return m_p == nullptr; }
  void MakeWeak(WeakHandleDestoryCallback callback = nullptr,
                void* data = nullptr) {
    Global::GetHC()->MakeWeak(reinterpret_cast<Object**>(m_p), callback, data);
  }
  bool IsWeak() const {
    return Global::GetHC()->IsWeak(reinterpret_cast<Object**>(m_p));
  }
  void Dispose() {
    Global::GetHC()->CloseGlobalSlot(reinterpret_cast<Object**>(m_p));
    ASSERT(m_p == nullptr);
  }
};

// template <>
// class Handle<Integer> {
//  Integer* m_p;
//
// public:
//  Handle() { m_p = nullptr; }
//  explicit Handle(Integer* p) { m_p = p; }
//  Handle(const Handle<Integer>& other) { m_p = other.m_p; }
//  Integer* ptr() const { return m_p; }
//  Integer* operator->() { return m_p; }
//  Integer** location() {
//    ASSERT(0);
//    return nullptr;
//  }
//  bool empty() { return m_p == nullptr; }
//};
// template <>
// class Handle<Float> {
//  Float* m_p;
//
// public:
//  Handle() { m_p = nullptr; }
//  explicit Handle(Float* p) { m_p = p; }
//  Handle(const Handle<Float>& other) { m_p = other.m_p; }
//  Float* ptr() const { return m_p; }
//  Float* operator->() { return m_p; }
//  Float** location() {
//    ASSERT(0);
//    return nullptr;
//  }
//  bool empty() { return m_p == nullptr; }
//};
// template <class T>
// class GlobalHandle : public Handle<T> {
// public:
//  GlobalHandle() : Handle<T>() {}
//  explicit GlobalHandle(T* p) : Handle<T>(HandleContainer::OpenGlobalSlot(p))
//  {}
//  //template <class TOther>
//  //GlobalHandle(const GlobalHandle<TOther>& other) : Handle(other) {}
//  template <class TOther>
//  GlobalHandle(const Handle<TOther>& other) :
//  Handle<T>(HandleContainer::OpenGlobalSlot(other.ptr())) {}
//
//  void MakeWeak() { HandleContainer::MakeWeak(Handle<T>::location()); }
//};
// template <>
// class GlobalHandle<Integer> : public Handle<Integer> {
// public:
//  GlobalHandle() : Handle<Integer>() {}
//  explicit GlobalHandle(Integer* p) : Handle<Integer>(p) {}
//  GlobalHandle(const GlobalHandle<Integer>& other) : Handle(other) {}
//  GlobalHandle(const Handle<Integer>& other) : Handle<Integer>(other.ptr()) {}
//  void MakeWeak() {}
//};
// template <>
// class GlobalHandle<Float> : public Handle<Float> {
// public:
//  GlobalHandle() : Handle<Float>() {}
//  explicit GlobalHandle(Float* p) : Handle<Float>(p) {}
//  GlobalHandle(const GlobalHandle<Float>& other) : Handle(other) {}
//  GlobalHandle(const Handle<Float>& other) : Handle<Float>(other.ptr()) {}
//  void MakeWeak() {}
//};

}  // namespace internal
}  // namespace rapid