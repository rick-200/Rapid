#pragma once
#include <cstdio>

namespace rapid {
namespace internal {


#ifdef  _DEBUG


#define DBG_LOG(s, ...) fprintf(stderr, "[%s %d]" s, __FUNCTION__, __LINE__, __VA_ARGS__)

#else

#define DBG_LOG(s, ...) ((void)0)

#endif  //  _DEBUG


}  // namespace internal
}  // namespace rapid