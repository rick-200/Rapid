#include "engine.h"

#include "global.h"
namespace rapid {

namespace i = internal;
using G = i::Global;
Engine* Engine::Init(Engine* engine) {
  G::Init((i::GlobalData*)engine);
  return Get();
}

Engine* Engine::Get() { return (Engine*)G::Get(); }

void Engine::Destory() { G::Close(); }


}  // namespace rapid