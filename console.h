#pragma once
#include "handle.h"
#include "object.h"
namespace rapid {
namespace internal {
namespace stdmodule {
Handle<Object> GetConsoleModule();
}
}  // namespace internal
}  // namespace rapid