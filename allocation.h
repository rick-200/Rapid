#pragma once
namespace rapid {
namespace internal {

class Malloced {
 public:
  // static void* operator new(size_t size) { return New(size); }
  // static void operator delete(void* p) { Free(p); }
  static void *Alloc(size_t size);
  static void Free(void *p);
};

}  // namespace internal
}  // namespace rapid