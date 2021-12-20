#pragma once
namespace rapid {
namespace internal {

class Malloced {
 public:
  static void* operator new(size_t size) { return New(size); }
  static void operator delete(void* p) { Delete(p); }
  static void* New(size_t size);
  static void Delete(void* p);
};

}  // namespace internal
}  // namespace rapid