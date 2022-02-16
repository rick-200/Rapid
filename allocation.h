#pragma once
#include <cstdlib>
namespace rapid {
namespace internal {

class Malloced {
 public:
  // static void* operator new(size_t size) { return New(size); }
  // static void operator delete(void* p) { Free(p); }
  static void *Alloc(size_t size);
  static void Free(void *p);
};

// malloc�ļ򵥷�װ�������Զ��ṩ������Ϣ
//����cnt*sizeof(T)�ֽڵ��ڴ�
//��ȫ����malloc/free
template <class T>
__declspec(allocator)
    [[nodiscard]] inline T *Allocate(size_t cnt = 1) noexcept {
  void *p = malloc(sizeof(T) * cnt);
  if (p == nullptr) {
    abort();
  }
  return reinterpret_cast<T *>(p);
}
// malloc�ļ򵥷�װ�������Զ��ṩ������Ϣ
//����size�ֽڵ��ڴ�
//��ȫ����malloc/free
template <class T>
__declspec(allocator)
    [[nodiscard]] inline T *AllocateSize(size_t size) noexcept {
  void *p = malloc(size);
  if (p == nullptr) {
    abort();
  }
  return reinterpret_cast<T *>(p);
}
inline void Free(void *p) { free(p); }

}  // namespace internal
}  // namespace rapid