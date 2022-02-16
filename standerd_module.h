#pragma once
#include "handle.h"
#include "object.h"
#include "type.h"
namespace rapid {
namespace internal {
namespace stdmodule {
Handle<Object> GetConsoleModule();
}
}  // namespace internal
}  // namespace rapid