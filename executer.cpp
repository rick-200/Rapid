#include "executer.h"
#include "bytecode.h"
#include "list.h"
namespace rapid {
namespace internal {

class Stack {
  Object *m_data;
  size_t size;
};
struct CallInfo {
  SharedFunctionData *sfd;
  Object *this_obj;
  Object **base;
  Object **top;
  byte *pc;
};
#define I(_obj) (Integer::cast(_obj)->value())
#define F(_obj) (Float::cast(_obj)->value())
#define MI(_obj)
inline Object *Wrap(int64_t v) { return Integer::FromInt64(v); }
inline Object *Wrap(double v) { return Float::FromDouble(v); }
inline Object *Add(Object *a, Object *b) {
  if (a->IsInteger()) {
    if (b->IsInteger()) {
      return Wrap(I(a) + I(b));
    } else if (b->IsFloat()) {
      return Wrap(I(a) + F(b));
    }
  } else if (a->IsFloat()) {
    if (b->IsInteger()) {
      return Wrap(F(a) + I(b));
    } else if (b->IsFloat()) {
      return Wrap(F(a) + F(b));
    }
  }
  if (a->IsString() && b->IsString()) {
    // TODO:
    VERIFY(0);
  }
  return Failure::Exception;
}
inline Object *Sub(Object *a, Object *b) {
  if (a->IsInteger()) {
    if (b->IsInteger()) {
      return Wrap(I(a) - I(b));
    } else if (b->IsFloat()) {
      return Wrap(I(a) - F(b));
    }
  } else if (a->IsFloat()) {
    if (b->IsInteger()) {
      return Wrap(F(a) - I(b));
    } else if (b->IsFloat()) {
      return Wrap(F(a) - F(b));
    }
  }
  return Failure::Exception;
}
inline Object *Mul(Object *a, Object *b) {
  if (a->IsInteger()) {
    if (b->IsInteger()) {
      return Wrap(I(a) * I(b));
    } else if (b->IsFloat()) {
      return Wrap(I(a) * F(b));
    }
  } else if (a->IsFloat()) {
    if (b->IsInteger()) {
      return Wrap(F(a) * I(b));
    } else if (b->IsFloat()) {
      return Wrap(F(a) * F(b));
    }
  }
  return Failure::Exception;
}
inline Object *IDiv(Object *a, Object *b) {
  if (!a->IsInteger() || !b->IsInteger()) {
    // TODO:
    return Failure::Exception;
  }
  return Wrap(I(a) / I(b));
}
inline Object *FDiv(Object *a, Object *b) {
  double da, db;
  if (a->IsInteger()) {
    da = I(a);
  } else if (a->IsFloat()) {
    da = F(a);
  } else {
    // TODO:
    return Failure::Exception;
  }
  if (b->IsInteger()) {
    db = I(b);
  } else if (b->IsFloat()) {
    db = F(b);
  } else {
    // TODO:
    return Failure::Exception;
  }
  return Wrap(da / db);
}

class ExecuterImpl : public Executer {
  struct {
    Object **p;
    size_t size;
  } m_stack;
  List<CallInfo> list_ci;
  Exception *err;

private:
  //执行最顶上的CallInfo
  void Execute() {
  l_begin:
    CallInfo *ci = &list_ci.back();
    byte *pc = ci->pc;
    Object **top = ci->top;
    FixedArray *kpool = ci->sfd->kpool;
    uint8_t u8;
    int16_t s16;
    while (true) {
      Opcode op = (Opcode)*pc;
      ++pc;
      switch (op) {
      case Opcode::NOP:

        break;
      case Opcode::ADD:
        --top;
        top[0] = Add(top[0], top[1]);
        break;
      case Opcode::SUB:
        --top;
        top[0] = Add(top[0], top[1]);
        break;
      case Opcode::MUL:
        --top;
        top[0] = Add(top[0], top[1]);
        break;
      case Opcode::IDIV:
        --top;
        top[0] = Add(top[0], top[1]);
        break;
      case Opcode::JMP:

      default:
        ASSERT(0);
      }
    }
  }

public:
  static ExecuterImpl *Create() {
    ExecuterImpl *p = (ExecuterImpl *)Heap::RawAlloc(sizeof(ExecuterImpl));
    p->err = nullptr;
    return p;
  }
  static void Destory(Executer *p) {
    // TODO;
  }

public:
  void TraceStack(GCTracer *gct) {}
  void CallRapidFunc(Handle<FunctionData> func, Handle<Object> *params,
                     size_t param_cnt) {}
  void ThrowException(Handle<Exception> e) {
    if (err != nullptr) {
      VERIFY(0);
    }
    err = *e;
  }
  Handle<Exception> GetException() { return Handle<Exception>(err); }
  bool HasException() { return err != nullptr; }
};

Executer *Executer::Create() { return ExecuterImpl::Create(); }

void Executer::Destory(Executer *p) { return ExecuterImpl::Destory(p); }
#define CALL_EXECUTER_IMPL(_f, ...)                                            \
  (((ExecuterImpl*)Global::GetExecuter())->_f(__VA_ARGS__))
void Executer::TraceStack(GCTracer *gct) {
  return CALL_EXECUTER_IMPL(TraceStack, gct);
}
void Executer::ThrowException(Handle<Exception> e) {
  return CALL_EXECUTER_IMPL(ThrowException, e);
}
Handle<Exception> Executer::GetException() {
  return CALL_EXECUTER_IMPL(GetException);
}
bool Executer::HasException() { return CALL_EXECUTER_IMPL(HasException); }
} // namespace internal
} // namespace rapid