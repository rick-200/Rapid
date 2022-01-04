#include "allocation.h"

#include <malloc.h>

#include "preprocessors.h"
namespace rapid {
namespace internal {
void* Malloced::Alloc(size_t size) {
  void* p = malloc(size);
  VERIFY(p != nullptr);
  return p;
}
void Malloced::Free(void* p) { free(p); }
}  // namespace internal
}  // namespace rapid