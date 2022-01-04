#include "executer.h"

#include <new>

#include "bytecode.h"
#include "config.h"
#include "factory.h"
#include "list.h"
namespace rapid {
namespace internal {

class Stack {
  Object *m_data;
  size_t size;
};
enum class CallType {
  FullCall,       //调用FunctionData
  StatelessCall,  //调用SharedFunctionData，即函数不访问外部变量
  NativeCall,     //调用NativeFunction
};
struct CallInfo {
  CallType t;
  union {
    FunctionData *fd;
    SharedFunctionData *sfd;
    NativeFunction *nf;
  };
  Object *this_object;
  Object **base;
  Object **top;  // top指向栈顶元素（不是栈顶+1）
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
#define DEF_INT_OP(_name, _op)                 \
  inline Object *_name(Object *a, Object *b) { \
    if (!a->IsInteger() || !b->IsInteger()) {  \
      return Failure::Exception; /*TODO*/      \
    }                                          \
    return Wrap(I(a) _op I(b));                \
  }
DEF_INT_OP(Mod, %);
DEF_INT_OP(BAnd, &);
DEF_INT_OP(BOr, |);
DEF_INT_OP(BXor, ^);
DEF_INT_OP(Shl, <<);
DEF_INT_OP(Shr, >>);
inline Object *And(Object *a, Object *b) {
  if (!a->IsBool() || !b->IsBool()) {
    return Failure::Exception; /*TODO*/
  }
  return (a->IsTrue() && b->IsTrue()) ? Heap::TrueValue() : Heap::FalseValue();
}
inline Object *Or(Object *a, Object *b) {
  if (!a->IsBool() || !b->IsBool()) {
    return Failure::Exception; /*TODO*/
  }
  return (a->IsTrue() || b->IsTrue()) ? Heap::TrueValue() : Heap::FalseValue();
}
inline Object *Not(Object *a) {
  if (!a->IsBool()) {
    return Failure::Exception; /*TODO*/
  }
  return (a->IsFalse()) ? Heap::TrueValue() : Heap::FalseValue();
}
inline Object *BNot(Object *a) {
  if (!a->IsInteger()) {
    return Failure::Exception; /*TODO*/
  }
  return Wrap(~I(a));
}
#define DEF_CMP_OP(_name, _op)                                         \
  inline Object *_name(Object *a, Object *b) {                         \
    if (a->IsInteger()) {                                              \
      if (!b->IsInteger()) {                                           \
        return Failure::Exception; /*TODO*/                            \
      }                                                                \
      return (I(a) _op I(b)) ? Heap::TrueValue() : Heap::FalseValue(); \
    } else if (a->IsFloat()) {                                         \
      if (!b->IsFloat()) {                                             \
        return Failure::Exception; /*TODO*/                            \
      }                                                                \
      return (F(a) _op F(b)) ? Heap::TrueValue() : Heap::FalseValue(); \
    } else {                                                           \
      return Failure::Exception; /*TODO*/                              \
    }                                                                  \
  }
DEF_CMP_OP(Less, <);
DEF_CMP_OP(LessEq, <=);
DEF_CMP_OP(Greater, >);
DEF_CMP_OP(GreaterEq, >=);
DEF_CMP_OP(Equal, ==);
DEF_CMP_OP(NotEqual, !=);
#define EXEC_BINOP(_name) \
  --top;                  \
  top[0] = _name(top[0], top[1]);
#define EXEC_UNOP(_name) top[0] = _name(top[0]);

class ExecuterImpl : public Executer {
  struct {
    Object **p;
    size_t size;
  } m_stack;
  List<CallInfo> list_ci;
  Exception *err;

 private:
  //预留足够的栈空间
  //在顶部的CallInfo::top基础上进行预留
  //若栈空间不足，最少扩充2倍
  void reserve_stack(size_t size) {
    if (list_ci.size() == 0) {
      if (m_stack.size < size) {
        m_stack.size = std::max(m_stack.size << 1, size);
        m_stack.p = (Object **)Heap::RawAlloc(m_stack.size);
      }
    } else {
      size = list_ci.back().top + size - m_stack.p +
             1 /*[m_stack.p,top]闭区间*/;  //实际总需求空间
      if (m_stack.size < size) {
        m_stack.size = std::max(m_stack.size << 1, size);
        Object **old_p = m_stack.p;
        m_stack.p = (Object **)Heap::RawAlloc(m_stack.size);
        memcpy(
            m_stack.p, old_p,
            sizeof(Object *) * (list_ci.back().top - m_stack.p + 1));  //复制栈
        for (auto &ci : list_ci) {  //修正栈指针
          ci.base = m_stack.p + (old_p - ci.base);
          ci.top = m_stack.p + (old_p - ci.top);
        }
        Heap::RawFree(old_p);
      }
    }
  }

  //若fd为null，填充FullCall；否则填充StatelessCall，且sfd需等于fd->shared_data
  void fill_runtime_info(CallInfo *ci, FunctionData *fd,
                         SharedFunctionData *sfd, Object **base) {
    if (fd != nullptr) {
      ASSERT(fd->shared_data == sfd);
      ci->t = CallType::FullCall;
      ci->fd = fd;
    } else {
      ci->t = CallType::StatelessCall;
      ci->sfd = sfd;
    }
    ci->base = base;
    ci->top = base + sfd->param_cnt;
    ci->pc = sfd->instructions->begin();
  }

  void push_callinfo(FunctionData *fd, SharedFunctionData *sfd,
                     size_t base_offset /*栈底，即第一个参数位置*/) {
    reserve_stack(sfd->max_stack);
    CallInfo new_ci;
    fill_runtime_info(&new_ci, fd, sfd, m_stack.p + base_offset);
    new_ci.this_object = Heap::NullValue();
    list_ci.push(new_ci);
  }

  //处理调用参数，若无法成功处理，TODO:抛出错误并返回false
  bool process_params(Object **param, size_t cnt, SharedFunctionData *sfd) {
    if (sfd->param_cnt < cnt) {
      return false;
    }
    Object *null_v = Heap::NullValue();
    for (size_t i = cnt; i < sfd->param_cnt; i++) param[i] = null_v;
    return true;
  }

  //执行最顶上的CallInfo，直到CallType为NativeCall
  void Execute() {
  l_begin:
    CallInfo *ci = &list_ci.back();
    byte *pc = ci->pc;
    // top指向栈顶元素（不是栈顶+1）
    Object **top = ci->top;
    Object **base = ci->base;
    Object **kpool;
    Object **inner_func;
    if (ci->t == CallType::FullCall) {
      kpool = ci->fd->shared_data->kpool->begin();
      inner_func = ci->fd->shared_data->inner_func->begin();
    } else if (ci->t == CallType::StatelessCall) {
      kpool = ci->sfd->kpool->begin();
      inner_func = ci->sfd->inner_func->begin();
    } else {
      ASSERT(ci->t == CallType::NativeCall);
      return;
    }

    while (true) {
      Opcode op = (Opcode)*pc;
      ++pc;
      switch (op) {
        case Opcode::NOP:
          break;
        case Opcode::LOADL:
          *++top = base[*(uint16_t *)pc];
          pc += 2;
          break;
        case Opcode::STOREL:
          base[*(uint16_t *)pc] = *top--;
          pc += 2;
          break;
        case Opcode::LOADK:
          *++top = kpool[*(uint16_t *)pc];
          pc += 2;
          break;
        case Opcode::LOAD_THIS:
          *++top = ci->this_object;
          break;
        case Opcode::COPY:
          top[1] = top[0];
          ++top;
          break;
        case Opcode::POP:
          --top;
          break;

        case Opcode::ADD:
          EXEC_BINOP(Add);
          break;
        case Opcode::SUB:
          EXEC_BINOP(Sub);
          break;
        case Opcode::MUL:
          EXEC_BINOP(Mul);
          break;
        case Opcode::IDIV:
          EXEC_BINOP(IDiv);
          break;
        case Opcode::FDIV:
          EXEC_BINOP(FDiv);
          break;
        case Opcode::MOD:
          EXEC_BINOP(Mod);
          break;
        case Opcode::BAND:
          EXEC_BINOP(BAnd);
          break;
        case Opcode::BOR:
          EXEC_BINOP(BOr);
          break;
        case Opcode::BNOT:
          EXEC_UNOP(BNot);
          break;
        case Opcode::BXOR:
          EXEC_BINOP(BXor);
          break;
        case Opcode::SHL:
          EXEC_BINOP(Shl);
          break;
        case Opcode::SHR:
          EXEC_BINOP(Shr);
          break;

        case Opcode::NOT:
          EXEC_UNOP(Not);
          break;
        case Opcode::AND:
          EXEC_BINOP(And);
          break;
        case Opcode::OR:
          EXEC_BINOP(Or);
          break;

        case Opcode::LT:  //<
          EXEC_BINOP(Less);
          break;
        case Opcode::GT:  //>
          EXEC_BINOP(Greater);
          break;
        case Opcode::LE:  //<=
          EXEC_BINOP(LessEq);
          break;
        case Opcode::GE:  //>=
          EXEC_BINOP(GreaterEq)
          break;
        case Opcode::EQ:  //==
          EXEC_BINOP(Equal)
          break;
        case Opcode::NEQ:  //!=
          EXEC_BINOP(NotEqual)
          break;

        case Opcode::GET_M:
          break;
        case Opcode::GET_I:
          break;
        case Opcode::SET_M:
          break;
        case Opcode::SET_I:
          break;

        case Opcode::JMP:
          --pc;
          pc -= *(int16_t *)pc;
          break;
        case Opcode::JMP_T:
          if (top[0]->IsTrue()) {
            --top;
            --pc;
            pc -= *(int16_t *)pc;  //跳转从JMP指令起始位开始
          } else {
            --top;
            pc += 2;
          }
          break;
        case Opcode::JMP_F:
          if (top[0]->IsFalse()) {
            --top;
            --pc;
            pc -= *(int16_t *)pc;  //跳转从JMP指令起始位开始
          } else {
            --top;
            pc += 2;
          }
          break;
        case Opcode::CALL: {  // func p1 p2 p3 [<- top]
          uint16_t cnt = *(uint16_t *)pc;
          pc += 2;
          top -= cnt;  //此时top指向要调用的对象，同时此位置也接受返回值
          list_ci.back().pc = pc;  //写回
          list_ci.back().base = base;
          list_ci.back().top = top;
          if (top[0]->IsFunctionData()) {
            FunctionData *fd = FunctionData::cast(top[0]);
            process_params(top + 1, cnt, fd->shared_data);
            push_callinfo(fd, fd->shared_data, top + 1 - m_stack.p);
            goto l_begin;
          } else if (top[0]->IsSharedFunctionData()) {  //无状态调用
            SharedFunctionData *sfd = SharedFunctionData::cast(top[0]);
            process_params(top + 1, cnt, sfd);
            push_callinfo(nullptr, sfd, top + 1 - m_stack.p);
            goto l_begin;
          } else if (top[0]->IsNativeFunction()) {
            Parameters param(nullptr, top + 1, cnt);
            top[0] = NativeFunction::cast(top[0])->call(param);
          } else {
            VERIFY(0);  // TODO
          }
          break;
        }
        case Opcode::THIS_CALL:
          VERIFY(0);  // TODO
          break;
        case Opcode::RET:
          list_ci.pop();
          list_ci.back().top[0] = top[0];  //返回值保存至上一层调用的栈顶
          goto l_begin;
          break;
        case Opcode::RETNULL:
          list_ci.pop();
          list_ci.back().top[0] =
              Heap::NullValue();  //返回值保存至上一层调用的栈顶
          goto l_begin;
          break;
        case Opcode::CLOSURE: {
          FunctionData *fd = *Factory::NewFunctionData();
          fd->shared_data =
              SharedFunctionData::cast(inner_func[*(uint16_t *)pc]);
          pc += 2;
          // fd->this_object = Heap::NullValue();
          *++top = fd;
          break;
        }
        case Opcode::CLOSURE_SELF: {
          FunctionData *fd = *Factory::NewFunctionData();
          fd->shared_data = ci->fd->shared_data;
          pc += 2;
          // fd->this_object = Heap::NullValue();
          *++top = fd;
          break;
        }
        default:
          ASSERT(0);
      }
    }
  }

  void fill_param(const RawParameters &param, Object **p,
                  SharedFunctionData *sfd) {
    if (param.count() <= sfd->param_cnt) {
      for (size_t i = 0; i < param.count(); i++) {
        p[i] = param.get(i);
      }
      Object *null_v = Heap::NullValue();
      for (size_t i = param.count(); i < sfd->param_cnt; i++) {
        p[i] = null_v;
      }
    } else {
      // TODO:extvar
      VERIFY(0);
    }
  }

  Object *CallFunction(FunctionData *fd, SharedFunctionData *sfd,
                       const RawParameters &param) {
    reserve_stack(sfd->max_stack + 1 /*返回值*/);
    Object **base;
    if (list_ci.empty()) {
      base = m_stack.p;
    } else {
      base = list_ci.back().top + 1;
    }

    CallInfo native_ci;
    native_ci.t = CallType::NativeCall;
    native_ci.base = base;
    native_ci.top = base;
    list_ci.push(native_ci);

    base += 1;
    fill_param(param, base, sfd);
    CallInfo ci;
    fill_runtime_info(&ci, fd, sfd, base);
    ci.this_object = param.get_this();
    list_ci.push(ci);

    Execute();
    ASSERT(list_ci.back().t == CallType::NativeCall);
    ASSERT(list_ci.back().base == native_ci.base);
    list_ci.pop();
    return *native_ci.base;
  }

 public:
  static ExecuterImpl *Create() {
    ExecuterImpl *p = (ExecuterImpl *)Heap::RawAlloc(sizeof(ExecuterImpl));
    p->m_stack.p = (Object **)Heap::RawAlloc(sizeof(Object *) *
                                             Config::InitialStackSlotCount);
    p->m_stack.size = Config::InitialStackSlotCount;
    new (&p->list_ci) List<CallInfo>();
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
  Handle<Object> CallFunction(Handle<SharedFunctionData> sfd,
                              const Parameters &param) {
    return Handle<Object>(CallFunction(nullptr, *sfd, param));
  }
  Handle<Object> CallFunction(Handle<FunctionData> fd,
                              const Parameters &param) {
    return Handle<Object>(CallFunction(*fd, fd->shared_data, param));
  }
};

Executer *Executer::Create() { return ExecuterImpl::Create(); }

void Executer::Destory(Executer *p) { return ExecuterImpl::Destory(p); }
#define CALL_EXECUTER_IMPL(_f, ...) \
  (((ExecuterImpl *)Global::GetExecuter())->_f(__VA_ARGS__))
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
Handle<Object> Executer::CallFunction(Handle<SharedFunctionData> sfd,
                                      const Parameters &param) {
  return CALL_EXECUTER_IMPL(CallFunction, sfd, param);
}
Handle<Object> Executer::CallFunction(Handle<FunctionData> fd,
                                      const Parameters &param) {
  return CALL_EXECUTER_IMPL(CallFunction, fd, param);
}
}  // namespace internal
}  // namespace rapid