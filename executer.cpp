#include "executer.h"

#include <new>

#include "bytecode.h"
#include "config.h"
#include "factory.h"
#include "list.h"
namespace rapid {
namespace internal {

//相对于整个栈最底部的偏移量
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
  //在top的基础上保留至少need个slot
  //注意：Reserve调用后，之前GetRawPtr返回值（可能）失效
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
struct CallInfo {
  bool is_script_call;  //从脚本代码执行的调用
  FunctionData *fd;
  Object *this_object;
  StackOffset base;  //栈底
  StackOffset top;   // top指向栈顶元素（不是栈顶+1）
                     //栈空间为[base,top]闭区间
  byte *pc;          //当前字节码指针
};

Array *NewArray(size_t reserved) {
  DEBUG_GC;
  Array *arr = Heap::AllocArray(reserved);  // TODO: 处理AllocArray失败的情况
  return arr;
}
Table *NewTable(size_t reserved) {
  DEBUG_GC;
  Table *tb = Heap::AllocTable(reserved);  // TODO: 处理AllocTable失败的情况
  return tb;
}
Exception *NewException(String *type, String *info, Object *data) {
  DEBUG_GC;
  Exception *e = Heap::AllocException(type, info, data);
  return e;
}
String *NewString(const char *str) {
  DEBUG_GC;
  String *s = Heap::AllocString(str, strlen(str));
  return s;
}
FunctionData *NewFunctionData(SharedFunctionData *sfd) {
  DEBUG_GC;
  FunctionData *fd = Heap::AllocFunctionData();
  fd->shared_data = sfd;
  fd->open_extvar_head = nullptr;
  fd->extvars = Heap::AllocFixedArray(sfd->extvars->length());
  return fd;
}
ExternVar *NewExternVar() {
  DEBUG_GC;
  ExternVar *ev = Heap::AllocExternVar();
  return ev;
}

#define I(_obj) (Integer::cast(_obj)->value())
#define F(_obj) (Float::cast(_obj)->value())
//#define MI(_obj)
inline Object *Wrap(int64_t v) { return Integer::FromInt64(v); }
inline Object *Wrap(double v) { return Float::FromDouble(v); }

// inline void ThrowArithmeticException() {
// String *s = NewString("ArithmeticException");
// Exception *e = NewException(NewString("ArithmeticException"), );
//}

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
  return Float::NaN();
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
  return Float::NaN();
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
  return Float::NaN();
}
inline Object *IDiv(Object *a, Object *b) {
  if (!a->IsInteger() || !b->IsInteger() || Integer::cast(b)->value() == 0) {
    return Float::NaN();
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
    return Float::NaN();
  }
  if (b->IsInteger()) {
    db = I(b);
  } else if (b->IsFloat()) {
    db = F(b);
  } else {
    return Float::NaN();
  }
  return Wrap(da / db);
}
#define DEF_INT_OP(_name, _op)                 \
  inline Object *_name(Object *a, Object *b) { \
    if (!a->IsInteger() || !b->IsInteger()) {  \
      return Float::NaN();                     \
    }                                          \
    return Wrap(I(a) _op I(b));                \
  }
DEF_INT_OP(Mod, %);
DEF_INT_OP(BAnd, &);
DEF_INT_OP(BOr, |);
DEF_INT_OP(BXor, ^);
DEF_INT_OP(Shl, <<);
DEF_INT_OP(Shr, >>);

inline Object *BNot(Object *a) {
  if (!a->IsInteger()) {
    return Float::NaN();
  }
  return Wrap(~I(a));
}

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

class ExecuterImpl : public Executer {
  ScriptStack m_stack;
  List<CallInfo> list_ci;
  Table *m_module;
  Exception *err;
  Object ***ptr_top;  //指向当前Execute栈上的top指针，用于在gc时确定边界
#if (DEBUG_LOG_EXEC_INFO)
  FILE *dbg_f;
#endif

 private:
  bool prepare_call(StackOffset top, size_t param_cnt, Object *this_obj,
                    FunctionData *fd) {
    if (fd->shared_data->param_cnt != param_cnt) {  //参数不匹配
      return false;
    }
    StackOffset new_base = AddOffset(top, 1);
    m_stack.Reserve(new_base, fd->shared_data->max_stack);

    CallInfo new_ci;
    new_ci.is_script_call = true;
    new_ci.fd = fd;
    new_ci.base = new_base;
    new_ci.top =
        AddOffset(new_base, fd->shared_data->param_cnt -
                                1 /*top为栈顶位置，栈长(top-base+1)，应减去1*/);
    new_ci.pc = fd->shared_data->instructions->begin();
    new_ci.this_object = this_obj;
    list_ci.push(new_ci);
  }
#if (DEBUG_LOG_EXEC_INFO)

  void dbg_print_stack() {
    fprintf(dbg_f, "----------stack---------\n");
    if (ptr_top != nullptr)
      list_ci.back().top = m_stack.GetOffset(*ptr_top);  //写回
    for (const auto &ci : list_ci) {
      fprintf(dbg_f, "CallInfo: ");
      if (ci.is_script_call) {
        fprintf(dbg_f, "FullCall<%p:'%s'@%p>\n", ci.fd,
                ci.fd->shared_data->name->cstr(), ci.fd->shared_data);
      } else {
        fprintf(dbg_f, "NativeCall\n");
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

  void close_extern_var(ExternVar **head, Object **target_ref) {
    ExternVar *p = *head;
    if (p == nullptr) return;
    if (p->value_ref == target_ref) {
      *head = p->un.next;
      p->un.value = *target_ref;
      p->value_ref = &p->un.value;
      return;
    }
    if (p->un.next == nullptr) return;
    ExternVar *ppre = p;
    p = p->un.next;
    while (p != nullptr) {
      ASSERT(p->is_open());
      if (p->value_ref == target_ref) {
        ppre->un.next = p->un.next;
        p->un.value = *target_ref;
        p->value_ref = &p->un.value;
        return;
      }
      ppre = p;
      p = p->un.next;
    }
  }
  void close_all_extern_var(ExternVar **head) {
    ExternVar *p = *head;
    while (p != nullptr) {
      ExternVar *pnxt = p->un.next;
      p->un.value = *p->value_ref;
      p->value_ref = &p->un.value;
      p = pnxt;
    }
    *head = nullptr;
  }

  //执行最顶上的CallInfo，直到CallType为NativeCall
  void Execute() {
  l_begin:
    CallInfo *ci = &list_ci.back();
    if (!ci->is_script_call) return;
    byte *pc = ci->pc;
    // top指向栈顶元素（不是栈顶+1）
    // top还用于标识gc范围，应在每条指令结束后再调整top指针
    Object **top = m_stack.GetRawPtr(ci->top);
    ptr_top = &top;  //更新ptr_top，用于gc
    Object **base = m_stack.GetRawPtr(ci->base);
    Object **kpool = ci->fd->shared_data->kpool->begin();
    Object **inner_func = ci->fd->shared_data->inner_func->begin();
    ExternVar **extern_var =
        reinterpret_cast<ExternVar **>(ci->fd->extvars->begin());
    while (true) {
#if (DEBUG_LOG_EXEC_INFO)
      char buff[32];
      read_bytecode(pc, buff);
      fprintf(dbg_f, "---->exec: %s\n", buff);
      fflush(dbg_f);
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
        case Opcode::STOREE:
          *extern_var[*(uint16_t *)pc]->value_ref = *top--;
          pc += 2;
          break;
        case Opcode::LOADK:
          *++top = kpool[*(uint16_t *)pc];
          pc += 2;
          break;
        case Opcode::LOADE:
          *++top = *extern_var[*(uint16_t *)pc]->value_ref;
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
        case Opcode::CLOSE:
          close_extern_var(&ci->fd->open_extvar_head, base + *(uint16_t *)pc);
          pc += 2;
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
          Array *arr = NewArray(cnt);  //此函数可能触发GC，此之前top不能变
          top -= cnt - 1;
          arr->quick_init(top, cnt);
          top[0] = arr;
          break;
        }
        case Opcode::MAKE_ARRAY_0:
          top[1] = NewArray(0);
          ++top;
          break;
        case Opcode::MAKE_TABLE: {
          uint16_t cnt = *(uint16_t *)pc;
          pc += 2;
          Table *tb = NewTable(cnt);  //此函数可能触发GC，此之前top不能变
          top -= cnt * 2 - 1;
          tb->quick_init(top, cnt);
          top[0] = tb;
          break;
        }
        case Opcode::MAKE_TABLE_0:
          top[1] = NewTable(4);
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
          pc += *(int16_t *)pc - 1;  //跳转从JMP指令起始位开始
          break;
        case Opcode::JMP_T:
          if (top[0]->IsTrue()) {
            --top;
            pc += *(int16_t *)pc - 1;  //跳转从JMP指令起始位开始
          } else {
            --top;
            pc += 2;
          }
          break;
        case Opcode::JMP_F:
          if (top[0]->IsFalse()) {
            --top;
            pc += *(int16_t *)pc - 1;  //跳转从JMP指令起始位开始
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
          ci->pc = pc;  //写回pc
          // ci->base = m_stack.GetOffset(base);
          ASSERT(ci->base == m_stack.GetOffset(base));  // base应不变

          Object *this_obj;
          Object *func;
          if (op == Opcode::CALL) {
            this_obj = nullptr;
            func = *(top - cnt);
            top -= cnt;  //此时top指向要调用的对象，同时此位置也接受返回值
          } else {  // op == Opcode::THIS_CALL
            ASSERT(op == Opcode::THIS_CALL);
            this_obj = *(top - cnt - 1);
            String *name = String::cast(*(top - cnt));
            VERIFY(this_obj->IsHeapObject());  // TODO:throw
            if (true) {
              Parameters param(this_obj, top - cnt + 1, cnt);
              //先不改变top，以免参数被gc回收
              ci->top = m_stack.GetOffset(top);  //写回top
              Object *ret = this_obj->InvokeMemberFunc(name, param);
              top -= cnt + 1;
              top[0] = ret;
              break;
            } else {  // TODO: CustomObject的THIS_CALL
              ASSERT(0);
              top -= cnt + 1;  //此时top指向要调用的对象，同时此位置也接受返回值
            }
          }

          ci->top = m_stack.GetOffset(top);  //写回top

          StackOffset off_base = m_stack.GetOffset(base);
          StackOffset off_top = m_stack.GetOffset(top);
          if (top[0]->IsFunctionData()) {
            FunctionData *fd = FunctionData::cast(top[0]);
            prepare_call(off_top, cnt, this_obj, fd);
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
          close_all_extern_var(&ci->fd->open_extvar_head);
          list_ci.pop();
          m_stack.GetRawPtr(list_ci.back().top)[0] =
              top[0];  //返回值保存至上一层调用的栈顶
          goto l_to_begin;
          break;
        case Opcode::RETNULL:
          close_all_extern_var(&ci->fd->open_extvar_head);
          list_ci.pop();
          m_stack.GetRawPtr(list_ci.back().top)[0] =
              nullptr;  //返回值保存至上一层调用的栈顶
          goto l_to_begin;
          break;
        case Opcode::CLOSURE: {
          FunctionData *fd = NewFunctionData(
              SharedFunctionData::cast(inner_func[*(uint16_t *)pc]));
          pc += 2;
          *++top = fd;
          for (size_t i = 0; i < fd->shared_data->extvars->length(); i++) {
            ExternVarInfo *evi =
                ExternVarInfo::cast(fd->shared_data->extvars->get(i));
            if (!evi->in_stack) {  //在此函数的外部变量数组中
              fd->extvars->set(i, ci->fd->extvars->get(evi->pos));
              continue;
            }

            //在此函数的栈上
            //先在打开的外部变量链表中搜索

            ExternVar *p = ci->fd->open_extvar_head;
            Object **stack_value_ref = base + evi->pos;
            while (p != nullptr && p->value_ref != stack_value_ref) {
              p = p->un.next;
            }
            if (p != nullptr &&
                p->value_ref ==
                    stack_value_ref) {  //在打开的外部变量链表中，直接添加

              fd->extvars->set(i, p);
            } else {  //不在打开的外部变量链表中，创建并添加到链表
              ExternVar *ev = NewExternVar();
              ev->value_ref = stack_value_ref;
              ev->un.next = ci->fd->open_extvar_head;
              ci->fd->open_extvar_head = ev;
              fd->extvars->set(i, ev);
            }
          }
          break;
        }
        // case Opcode::CLOSURE_SELF: {
        //  ASSERT(ci->t == CallType::FullCall ||
        //         ci->t == CallType::StatelessCall);
        //  SharedFunctionData *sfd =
        //      ci->t == CallType::FullCall ? ci->fd->shared_data : ci->sfd;
        //  if (sfd->extvars->length() == 0) {
        //    *++top = sfd;
        //  } else {
        //    FunctionData *fd = Heap::
        //        AllocFunctionData();  // TODO:处理AllocFunctionData失败的情况
        //    fd->shared_data = sfd;
        //    *++top = fd;
        //  }
        //  break;
        //}
        default:
          ASSERT(0);
      }
#if (DEBUG_LOG_EXEC_INFO)
      dbg_print_stack();
#endif
    }
    goto l_return;

  l_exception:

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

  Object *CallFunction(FunctionData *fd, const RawParameters &param) {
    if (fd->shared_data->param_cnt != param.count()) {
      VERIFY(0);  // TODO;
    }
    StackOffset base;
    if (list_ci.empty()) {
      base = static_cast<StackOffset>(0);
    } else {
      base = AddOffset(list_ci.back().top, 1);
    }
    m_stack.Reserve(base, fd->shared_data->max_stack + 1 /*返回值*/);

    CallInfo native_ci;
    native_ci.is_script_call = false;
    native_ci.base = base;
    native_ci.top = base;
    m_stack.GetRawPtr(base)[0] = nullptr;
    list_ci.push(native_ci);

    prepare_call(base, param.count(), param.get_this(), fd);

    base = AddOffset(base, 1);

    Object **pbase = m_stack.GetRawPtr(base);
    for (size_t i = 0; i < param.count(); i++) pbase[i] = param[i];

    Execute();

    ASSERT(!list_ci.back().is_script_call);
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
      ASSERT(list_ci.back().is_script_call);
      // ASSERT(list_ci.back().top<=)
      list_ci.back().top = m_stack.GetOffset(*ptr_top);  //写回
    }
    for (const auto &ci : list_ci) {
      if (ci.is_script_call) gct->Trace(ci.fd);
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
  Handle<Object> CallFunction(Handle<FunctionData> fd,
                              const Parameters &param) {
    return Handle<Object>(CallFunction(*fd, param));
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
Handle<Object> Executer::CallFunction(Handle<FunctionData> fd,
                                      const Parameters &param) {
  return CALL_EXECUTER_IMPL(CallFunction, fd, param);
}
void Executer::RegisterModule(Handle<String> name, Handle<Object> md) {
  return CALL_EXECUTER_IMPL(RegisterModule, name, md);
}
}  // namespace internal
}  // namespace rapid