#include "executer.h"

#include <new>

#include "bytecode.h"
#include "config.h"
#include "factory.h"
#include "list.h"
namespace rapid {
namespace internal {

//���������ջ��ײ���ƫ����
enum class StackOffset : uintptr_t {};
inline StackOffset AddOffset(StackOffset so, intptr_t offset) {
  return static_cast<StackOffset>(static_cast<intptr_t>(so) + offset);
}
class ScriptStack {
  Object **m_p;
  size_t m_size;

 public:
  ScriptStack() {
    m_size = Config::InitialStackSlotCount;
    m_p = (Object **)malloc(sizeof(Object *) * Config::InitialStackSlotCount);
  }
  Object **GetRawPtr(StackOffset so) {
    return m_p + static_cast<uintptr_t>(so);
  }
  StackOffset GetOffset(Object **p) {
    ASSERT(p >= m_p && p < m_p + m_size);
    return static_cast<StackOffset>(p - m_p);
  }
  //��top�Ļ����ϱ�������need��slot
  //ע�⣺Reserve���ú�֮ǰGetRawPtr����ֵ�����ܣ�ʧЧ
  void Reserve(StackOffset top, size_t need) {
    size_t new_size = (size_t)(GetRawPtr(top) + need - m_p);
    if (new_size <= m_size) return;
    if (new_size <= (m_size << 1)) new_size = m_size << 1;
    size_t old_size = m_size;
    Object **oldp = m_p;
    m_p = (Object **)malloc(sizeof(Object *) * m_size);
    VERIFY(m_p != nullptr);
    memcpy(m_p, oldp, sizeof(Object *) * old_size);
    free(oldp);
  }
};
class Stack {
  Object *m_data;
  size_t size;
};
enum class CallType {
  FullCall,       //����FunctionData
  StatelessCall,  //����SharedFunctionData���������������ⲿ����
  NativeCall,     //��C++���ýű����������ڻ�ȡ����ֵ
};
struct CallInfo {
  CallType t;
  union {
    FunctionData *fd;
    SharedFunctionData *sfd;
    NativeFunction *nf;
  };
  Object *this_object;
  StackOffset base;  //ջ��
  StackOffset top;   // topָ��ջ��Ԫ�أ�����ջ��+1��
  byte *pc;          //��ǰ�ֽ���ָ��
};
#define I(_obj) (Integer::cast(_obj)->value())
#define F(_obj) (Float::cast(_obj)->value())
//#define MI(_obj)
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
#define EXEC_BINOP(_name)           \
  top[-1] = _name(top[-1], top[0]); \
  --top;
#define EXEC_UNOP(_name) top[0] = _name(top[0]);

Array *NewArray(size_t reserved) {
  DEBUG_GC;
  Array *arr = Heap::AllocArray(reserved);  // TODO: ����AllocArrayʧ�ܵ����
  return arr;
}
Table *NewTable() {
  DEBUG_GC;
  Table *tb = Heap::AllocTable();  // TODO: ����AllocTableʧ�ܵ����
  return tb;
}

class ExecuterImpl : public Executer {
  ScriptStack m_stack;
  List<CallInfo> list_ci;
  Table *m_module;
  Exception *err;
  Object ***ptr_top;  //ָ��ǰExecuteջ�ϵ�topָ�룬������gcʱȷ���߽�
#if (DEBUG_LOG_EXEC_INFO)
  FILE *dbg_f;
#endif

 private:
  //��fd��Ϊnull�����FullCall����sfd�����fd->shared_data
  //�������StatelessCall
  void fill_runtime_info(CallInfo *ci, FunctionData *fd,
                         SharedFunctionData *sfd, StackOffset base) {
    if (fd != nullptr) {
      ASSERT(fd->shared_data == sfd);
      ci->t = CallType::FullCall;
      ci->fd = fd;
    } else {
      ci->t = CallType::StatelessCall;
      ci->sfd = sfd;
    }
    ci->base = base;
    ci->top = AddOffset(
        base, sfd->param_cnt - 1 /*topΪջ��λ�ã�ջ��(top-base+1)��Ӧ��ȥ1*/);
    ci->pc = sfd->instructions->begin();
  }

  bool prepare_call(StackOffset top, size_t param_cnt, Object *this_obj,
                    FunctionData *fd, SharedFunctionData *sfd) {
    if (sfd->param_cnt != param_cnt) {  //������ƥ��
      return false;
    }
    StackOffset new_base = AddOffset(top, 1);
    m_stack.Reserve(new_base, sfd->max_stack);
    CallInfo new_ci;
    fill_runtime_info(&new_ci, fd, sfd, new_base);
    new_ci.this_object = this_obj;
    list_ci.push(new_ci);
  }
#if (DEBUG_LOG_EXEC_INFO)

  void dbg_print_stack() {
    fprintf(dbg_f, "----------stack---------\n");
    if (ptr_top != nullptr)
      list_ci.back().top = m_stack.GetOffset(*ptr_top);  //д��
    for (const auto &ci : list_ci) {
      fprintf(dbg_f, "CallInfo: ");
      switch (ci.t) {
        case CallType::FullCall:
          fprintf(dbg_f, "FullCall<%p:'%s'@%p>\n", ci.fd,
                  ci.fd->shared_data->name->cstr(), ci.fd->shared_data);
          break;
        case CallType::StatelessCall:
          fprintf(dbg_f, "StatelessCall<'%s'@%p>\n", ci.sfd->name->cstr(),
                  ci.sfd);
          break;
        case CallType::NativeCall:
          fprintf(dbg_f, "NativeCall\n");
          break;
      }

      for (size_t i = static_cast<size_t>(ci.base);
           i <= static_cast<size_t>(ci.top); i++) {
        fprintf(dbg_f, "  [%llu:%llu] ", i, i - static_cast<size_t>(ci.base));
        debug_print(dbg_f, m_stack.GetRawPtr(static_cast<StackOffset>(i))[0]);
        fprintf(dbg_f, "\n");
      }
    }
    fprintf(dbg_f, "------------------------\n");
    fflush(dbg_f);
  }
#endif  //  _DEBUG

  //ִ����ϵ�CallInfo��ֱ��CallTypeΪNativeCall
  void Execute() {
  l_begin:
    CallInfo *ci = &list_ci.back();
    byte *pc = ci->pc;
    // topָ��ջ��Ԫ�أ�����ջ��+1��
    // top�����ڱ�ʶgc��Χ��Ӧ��ÿ��ָ��������ٵ���topָ��
    Object **top = m_stack.GetRawPtr(ci->top);
    ptr_top = &top;  //����ptr_top������gc
    Object **base = m_stack.GetRawPtr(ci->base);
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
      return;  //ֱ�ӷ���
    }

    while (true) {
#if (DEBUG_LOG_EXEC_INFO)
      char buff[32];
      read_bytecode(pc, buff);
      fprintf(dbg_f, "---->exec: %s\n", buff);
#endif  //  _DEBUG
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
        case Opcode::PUSH_NULL:
          ++top;
          top[0] = nullptr;
          break;
        case Opcode::POP:
          --top;
          break;
        case Opcode::POPN:
          top -= *(uint8_t *)pc;
          ++pc;
          break;
        case Opcode::IMPORT:
          if (m_module->exists(String::cast(top[0]))) {
            top[0] = m_module->get(String::cast(top[0]));
          } else {
            top[0] = nullptr;
          }
          break;
        case Opcode::MAKE_ARRAY: {
          uint16_t cnt = *(uint16_t *)pc;
          pc += 2;
          Array *arr = NewArray(cnt);
          for (Object **p = top - cnt + 1; p <= top; p++) {
            arr->push(*p);  //�˴����ᴥ��GC������arrδ���������ᱻ����
          }
          top -= cnt - 1;
          top[0] = arr;
          break;
        }
        case Opcode::MAKE_ARRAY_0:
          top[1] = NewArray(0);
          ++top;
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

        case Opcode::GET_P:
          top[-1] = top[-1]->GetProperty(String::cast(top[0]),
                                         AccessSpecifier::Public);
          --top;
          break;
        case Opcode::SET_P:
          top[-1]->SetProperty(String::cast(top[0]), top[-2],
                               AccessSpecifier::Public);
          --top;
          break;
        case Opcode::GET_I: {
          Parameters param(nullptr, top, 1);
          top[-1] = top[-1]->InvokeMetaFunc(MetaFunctionID::GET_INDEX, param);
          --top;
          break;
        }

        case Opcode::SET_I: {
          Object *p[] = {top[0], top[-2]};
          Parameters param(nullptr, p, 2);
          top[-2] = top[-1]->InvokeMetaFunc(MetaFunctionID::SET_INDEX, param);
          top -= 2;
          break;
        }

        case Opcode::JMP:
          pc += *(int16_t *)pc - 1;  //��ת��JMPָ����ʼλ��ʼ
          break;
        case Opcode::JMP_T:
          if (top[0]->IsTrue()) {
            --top;
            pc += *(int16_t *)pc - 1;  //��ת��JMPָ����ʼλ��ʼ
          } else {
            --top;
            pc += 2;
          }
          break;
        case Opcode::JMP_F:
          if (top[0]->IsFalse()) {
            --top;
            pc += *(int16_t *)pc - 1;  //��ת��JMPָ����ʼλ��ʼ
          } else {
            --top;
            pc += 2;
          }
          break;
        case Opcode::CALL:         // func p1 p2 p3 [<- top]
        case Opcode::THIS_CALL: {  // obj func_name p1 p2 p3 [<- top]
          uint16_t cnt = *(uint16_t *)pc;
          pc += 2;
          ASSERT(ci == &list_ci.back());
          ci->pc = pc;  //д��pc
          // ci->base = m_stack.GetOffset(base);
          ASSERT(ci->base == m_stack.GetOffset(base));  // baseӦ����

          Object *this_obj;
          Object *func;
          if (op == Opcode::CALL) {
            this_obj = nullptr;
            func = *(top - cnt);
            top -= cnt;  //��ʱtopָ��Ҫ���õĶ���ͬʱ��λ��Ҳ���ܷ���ֵ
          } else {  // op == Opcode::THIS_CALL
            ASSERT(op == Opcode::THIS_CALL);
            this_obj = *(top - cnt - 1);
            String *name = String::cast(*(top - cnt));
            VERIFY(this_obj->IsHeapObject());  // TODO:throw
            if (true) {
              Parameters param(this_obj, top - cnt + 1, cnt);
              //�Ȳ��ı�top�����������gc����
              ci->top = m_stack.GetOffset(top);  //д��top
              Object *ret = this_obj->InvokeMemberFunc(name, param);
              top -= cnt + 1;
              top[0] = ret;
              break;
            } else {  // TODO: CustomObject��THIS_CALL
              ASSERT(0);
              top -= cnt + 1;  //��ʱtopָ��Ҫ���õĶ���ͬʱ��λ��Ҳ���ܷ���ֵ
            }
          }

          ci->top = m_stack.GetOffset(top);  //д��top

          StackOffset off_base = m_stack.GetOffset(base);
          StackOffset off_top = m_stack.GetOffset(top);
          if (top[0]->IsFunctionData()) {
            FunctionData *fd = FunctionData::cast(top[0]);
            prepare_call(off_top, cnt, this_obj, fd, fd->shared_data);
            goto l_to_begin;
          } else if (top[0]->IsSharedFunctionData()) {  //��״̬����
            SharedFunctionData *sfd = SharedFunctionData::cast(top[0]);
            prepare_call(off_top, cnt, this_obj, nullptr, sfd);
            goto l_to_begin;
          } else if (top[0]->IsNativeFunction()) {
            Parameters param(this_obj, top + 1, cnt);
            top[0] = NativeFunction::cast(top[0])->call(param);
          } else {
            VERIFY(0);  // TODO
          }
          break;
        }
        case Opcode::RET:
          list_ci.pop();
          m_stack.GetRawPtr(list_ci.back().top)[0] =
              top[0];  //����ֵ��������һ����õ�ջ��
          goto l_to_begin;
          break;
        case Opcode::RETNULL:
          list_ci.pop();
          m_stack.GetRawPtr(list_ci.back().top)[0] =
              nullptr;  //����ֵ��������һ����õ�ջ��
          goto l_to_begin;
          break;
        case Opcode::CLOSURE: {
          SharedFunctionData *sfd =
              SharedFunctionData::cast(inner_func[*(uint16_t *)pc]);
          pc += 2;
          if (sfd->extvars->length() == 0) {
            *++top = sfd;
          } else {
            FunctionData *fd = Heap::
                AllocFunctionData();  // TODO:����AllocFunctionDataʧ�ܵ����
            fd->shared_data = sfd;
            *++top = fd;
          }
          break;
        }
        case Opcode::CLOSURE_SELF: {
          ASSERT(ci->t == CallType::FullCall ||
                 ci->t == CallType::StatelessCall);
          SharedFunctionData *sfd =
              ci->t == CallType::FullCall ? ci->fd->shared_data : ci->sfd;
          if (sfd->extvars->length() == 0) {
            *++top = sfd;
          } else {
            FunctionData *fd = Heap::
                AllocFunctionData();  // TODO:����AllocFunctionDataʧ�ܵ����
            fd->shared_data = sfd;
            *++top = fd;
          }
          break;
        }
        default:
          ASSERT(0);
      }
#if (DEBUG_LOG_EXEC_INFO)
      dbg_print_stack();
#endif
    }
    goto l_return;
  l_to_begin:
    ptr_top = nullptr;
#if (DEBUG_LOG_EXEC_INFO)
    dbg_print_stack();
#endif
    goto l_begin;
  l_return:;
    ptr_top = nullptr;
  }

  // void fill_param(const RawParameters &param, Object **p,
  //                SharedFunctionData *sfd) {
  //  if (param.count() <= sfd->param_cnt) {
  //    for (size_t i = 0; i < param.count(); i++) {
  //      p[i] = param.get(i);
  //    }
  //    Object *null_v = Heap::NullValue();
  //    for (size_t i = param.count(); i < sfd->param_cnt; i++) {
  //      p[i] = null_v;
  //    }
  //  } else {
  //    // TODO:extvar
  //    VERIFY(0);
  //  }
  //}

  Object *CallFunction(FunctionData *fd, SharedFunctionData *sfd,
                       const RawParameters &param) {
    if (sfd->param_cnt != param.count()) {
      VERIFY(0);  // TODO;
    }
    StackOffset base;
    if (list_ci.empty()) {
      base = static_cast<StackOffset>(0);
    } else {
      base = AddOffset(list_ci.back().top, 1);
    }
    m_stack.Reserve(base, sfd->max_stack + 1 /*����ֵ*/);

    CallInfo native_ci;
    native_ci.t = CallType::NativeCall;
    native_ci.base = base;
    native_ci.top = base;
    m_stack.GetRawPtr(base)[0] = nullptr;
    list_ci.push(native_ci);

    base = AddOffset(base, 1);

    Object **pbase = m_stack.GetRawPtr(base);
    for (size_t i = 0; i < param.count(); i++) pbase[i] = param[i];

    CallInfo ci;
    fill_runtime_info(&ci, fd, sfd, base);
    ci.this_object = param.get_this();
    list_ci.push(ci);

    Execute();

    ASSERT(list_ci.back().t == CallType::NativeCall);
    ASSERT(list_ci.back().base == native_ci.base);

    list_ci.pop();
    return *m_stack.GetRawPtr(native_ci.base);
  }

 public:
  static ExecuterImpl *Create() {
    ExecuterImpl *p = (ExecuterImpl *)malloc(sizeof(ExecuterImpl));
    p->ptr_top = nullptr;
    new (&p->m_stack) ScriptStack();
    new (&p->list_ci) List<CallInfo>();
    p->m_module = Factory::NewTable().ptr();
    p->err = nullptr;
#if (DEBUG_LOG_EXEC_INFO)
    p->dbg_f = fopen("./exec_log.txt", "w");
#endif  // _DEBUG
    return p;
  }
  static void Destory(Executer *p) {
    // TODO;
  }

 public:
  void TraceStack(GCTracer *gct) {
    gct->Trace(this->m_module);
    if (list_ci.empty()) return;
    if (ptr_top != nullptr) {
      ASSERT(list_ci.back().t != CallType::NativeCall);
      // ASSERT(list_ci.back().top<=)
      list_ci.back().top = m_stack.GetOffset(*ptr_top);  //д��
    }
    for (const auto &ci : list_ci) {
      if (ci.t == CallType::FullCall)
        gct->Trace(ci.fd);
      else if (ci.t == CallType::StatelessCall)
        gct->Trace(ci.sfd);
    }
    size_t top = static_cast<size_t>(list_ci.back().top);
    for (size_t i = static_cast<size_t>(0); i <= top; i++) {
      gct->Trace(*m_stack.GetRawPtr(static_cast<StackOffset>(i)));
    }
  }
  // void CallRapidFunc(Handle<FunctionData> func, Handle<Object> *params,
  //                   size_t param_cnt) {}
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
  void RegisterModule(Handle<String> name, Handle<Object> md) {
    m_module->set(*name, *md);
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
void Executer::RegisterModule(Handle<String> name, Handle<Object> md) {
  return CALL_EXECUTER_IMPL(RegisterModule, name, md);
}
}  // namespace internal
}  // namespace rapid