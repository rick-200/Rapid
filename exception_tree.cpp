#include "exception_tree.h"

#include <new>

#include "allocation.h"
#include "factory.h"
#include "global.h"
#include "list.h"
namespace rapid {
namespace internal {
class ExceptionTreeImpl {
  Array* m_types;
  List<uint64_t> m_parents;

 public:
  uint64_t Register(uint64_t parent_id, Handle<String> type) {
    VERIFY(parent_id < m_parents.size());
    ASSERT(m_types->length() == m_parents.size());
    uint64_t id = m_parents.size();
    m_parents.push(parent_id);
    m_types->push(*type);
    return id;
  }
  Handle<String> GetType(uint64_t id) {
    VERIFY(id < m_types->length());
    return Handle<String>(String::cast(m_types->get(id)));
  }
  bool IsSubexceptionOf(uint64_t id, uint64_t parent_id) {
    VERIFY(id < m_types->length());
    VERIFY(parent_id < m_types->length());
    if (id < parent_id) return false;  //子异常一定定义在父异常后面
    if (parent_id == 0 || id == parent_id) return true;
    while (id != 0) {
      id = m_parents[id];
      if (id == parent_id) return true;
    }
    return false;
  }
  void TraceMember(GCTracer* gct) { gct->Trace(m_types); }
  static ExceptionTree* Create() {
    ExceptionTreeImpl* eti = Allocate<ExceptionTreeImpl>();
    new (&eti->m_parents) List<uint64_t>();
    eti->m_types = *Factory::NewArray();
    eti->m_parents.push(0);
    eti->m_types->push(*Factory::NewString("Exception"));  // 0
    eti->Register(0, Factory::NewString("SyntaxError"));     // 1
    eti->Register(0, Factory::NewString("TypeError"));       // 2
    return reinterpret_cast<ExceptionTree*>(eti);
  }
};

#define ETI reinterpret_cast<ExceptionTreeImpl*>(Global::GetExceptionTree())
uint64_t ExceptionTree::Register(uint64_t parent_id, Handle<String> type) {
  return ETI->Register(parent_id, type);
}
Handle<String> ExceptionTree::GetType(uint64_t id) { return ETI->GetType(id); }
ExceptionTree* ExceptionTree::Create() {
  return reinterpret_cast<ExceptionTree*>(ExceptionTreeImpl::Create());
}
void ExceptionTree::TraceMember(GCTracer* gct) { return ETI->TraceMember(gct); }
bool ExceptionTree::IsSubexceptionOf(uint64_t id, uint64_t parent_id) {
  return ETI->IsSubexceptionOf(id, parent_id);
}
}  // namespace internal
}  // namespace rapid