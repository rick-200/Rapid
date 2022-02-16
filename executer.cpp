#include "executer.h"

#include <new>

#include "bytecode.h"
#include "config.h"
#include "factory.h"
#include "list.h"
#include "native_object_interface.h"
#include "stringbuilder.h"
namespace rapid {
namespace internal {
typedef Object *Slot;
struct CallInfo {
  bool is_script_call;  //�ӽű�����ִ�еĵ���
  FunctionData *fd;
  Slot *base;  //ջ��
  Slot *top;   // topָ��ջ��Ԫ�أ�����ջ��+1��
               //ջ�ռ�Ϊ[base,top]������
  byte *pc;    //��ǰ�ֽ���ָ��
};

Array *NewArray(size_t reserved) {
  DEBUG_GC;
  Array *arr = Heap::AllocArray(reserved);  // TODO: ����AllocArrayʧ�ܵ����
  return arr;
}
Dictionary *NewDictionary(size_t reserved) {
  DEBUG_GC;
  Dictionary *tb =
      Heap::AllocDictionary(reserved);  // TODO: ����AllocDictionaryʧ�ܵ����
  return tb;
}
// Exception *NewException(String *type, String *info, Object *data) {
//  DEBUG_GC;
//  Exception *e = Heap::AllocException(type, info, data);
//  return e;
//}
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
Table *NewTable(TableInfo *ti, Object **vals) {
  DEBUG_GC;
  Table *t = Heap::AllocTable(ti);
  for (size_t i = 0; i < ti->prop_count(); i++) t->set(i, vals[i]);
  return t;
}

Object *GetProperty(Object *obj, String *name) {
  if (!obj->IsHeapObject()) {
    return value_failure_normal;
  }
  switch (HeapObject::cast(obj)->type()) {
    case HeapObjectType::String: {
      if (String::Equal(name, "length")) {
        return Integer::FromInt64(String::cast(obj)->length());
      }
      return value_failure_normal;
    }
    case HeapObjectType::Array: {
      if (String::Equal(name, "length")) {
        return Integer::FromInt64(Array::cast(obj)->length());
      }
      return value_failure_normal;
    }
    case HeapObjectType::Dictionary: {
      return value_failure_normal;
    }
    case HeapObjectType::Exception: {
      if (String::Equal(name, "id")) {
        return Integer::FromInt64(Exception::cast(obj)->id);
      } else if (String::Equal(name, "type")) {
        HandleScope hs;
        return *ExceptionTree::GetType(Exception::cast(obj)->id);
      } else if (String::Equal(name, "info")) {
        return Exception::cast(obj)->info;
      }
      return value_failure_normal;
    }
    case HeapObjectType::Table: {
      size_t idx = Table::cast(obj)->table_info()->get_index(name);
      if (idx == TableInfo::invalid_index) return value_failure_normal;
      return Table::cast(obj)->get(idx);
    }
    case HeapObjectType::NativeObject: {
      return NativeObject::cast(obj)->interface()->get_prop(
          NativeObject::cast(obj)->data(), name);
    }
    default:
      return value_failure_normal;
  }
}
Object *SetProperty(Object *obj, String *name, Object *value) {
  if (!obj->IsHeapObject()) {
    return value_failure_normal;
  }
  switch (HeapObject::cast(obj)->type()) {
    case HeapObjectType::Table: {
      size_t idx = Table::cast(obj)->table_info()->get_index(name);
      if (idx == TableInfo::invalid_index) return value_failure_normal;
      Table::cast(obj)->set(idx, value);
      return nullptr;
    }
    case HeapObjectType::NativeObject: {
      return NativeObject::cast(obj)->interface()->set_prop(
          NativeObject::cast(obj)->data(), name, value);
    }
    default:
      return value_failure_normal;
  }
}
Object *GetIndex(Object *obj, Object *idx) {
  if (!obj->IsHeapObject()) {
    return value_failure_normal;
  }
  switch (HeapObject::cast(obj)->type()) {
    case HeapObjectType::String:
      return value_failure_normal;
    case HeapObjectType::Array: {
      if (idx->IsInteger())
        return Array::cast(obj)->get(Integer::cast(idx)->value());
      return value_failure_normal;
    }
    case HeapObjectType::Dictionary: {
      if (idx->IsString()) return Dictionary::cast(obj)->get(String::cast(idx));
      return value_failure_normal;
    }
    case HeapObjectType::Exception:
      return value_failure_normal;
    case HeapObjectType::Table:
      return value_failure_normal;
    case HeapObjectType::NativeObject:
      return value_failure_normal;
    default:
      return value_failure_normal;
  }
}
Object *SetIndex(Object *obj, Object *idx, Object *value) {
  if (!obj->IsHeapObject()) {
    return value_failure_normal;
  }
  switch (HeapObject::cast(obj)->type()) {
    case HeapObjectType::String:
      return value_failure_normal;
    case HeapObjectType::Array: {
      if (idx->IsInteger()) {
        Array::cast(obj)->set(Integer::cast(idx)->value(), value);
        return nullptr;
      }
      return value_failure_normal;
    }
    case HeapObjectType::Dictionary: {
      if (idx->IsString()) {
        return Dictionary::cast(obj)->set(String::cast(idx), value);
        return nullptr;
      }
      return value_failure_normal;
    }
    case HeapObjectType::Exception:
      return value_failure_normal;
    case HeapObjectType::Table:
      return value_failure_normal;
    case HeapObjectType::NativeObject:
      return value_failure_normal;
    default:
      return value_failure_normal;
  }
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
    // TODO: String�ļӷ�
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
    return value_failure_exception; /*TODO*/
  }
  return (a->IsTrue() && b->IsTrue()) ? value_true : value_false;
}
inline Object *Or(Object *a, Object *b) {
  if (!a->IsBool() || !b->IsBool()) {
    return value_failure_exception; /*TODO*/
  }
  return (a->IsTrue() || b->IsTrue()) ? value_true : value_false;
}
inline Object *Not(Object *a) {
  if (!a->IsBool()) {
    return value_failure_exception; /*TODO*/
  }
  return (a->IsFalse()) ? value_true : value_false;
}
#define DEF_CMP_OP(_name, _op)                           \
  inline Object *_name(Object *a, Object *b) {           \
    if (a->IsInteger()) {                                \
      if (!b->IsInteger()) {                             \
        return value_failure_exception; /*TODO*/         \
      }                                                  \
      return (I(a) _op I(b)) ? value_true : value_false; \
    } else if (a->IsFloat()) {                           \
      if (!b->IsFloat()) {                               \
        return value_failure_exception; /*TODO*/         \
      }                                                  \
      return (F(a) _op F(b)) ? value_true : value_false; \
    } else {                                             \
      return value_failure_exception; /*TODO*/           \
    }                                                    \
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
  List<CallInfo> list_ci;
  Dictionary *m_module;
  Exception *err;
#if (DEBUG_LOG_EXEC_INFO)
  FILE *dbg_f;
#endif
  struct {
    Object **p;
    size_t size;
  } m_stack;

 private:
  Slot *reserve_stack(Slot *base, size_t need) {
    size_t new_size = (size_t)(base + need - m_stack.p);
    if (new_size <= m_stack.size) return base;
    intptr_t ret_offset = base - m_stack.p;
    //��������һ��
    if (new_size <= (m_stack.size << 1)) new_size = m_stack.size << 1;
    size_t old_size = m_stack.size;
    Object **oldp = m_stack.p;
    m_stack.size = new_size;
    m_stack.p = Allocate<Slot>(m_stack.size);
    VERIFY(m_stack.p != nullptr);
    memcpy(m_stack.p, oldp, sizeof(Object *) * old_size);
    free(oldp);

    //��������ָ��ջ�����ָ��
    for (auto &ci : list_ci) {
      if (!ci.is_script_call) continue;
      ci.base = m_stack.p + (ci.base - oldp);
      ci.top = m_stack.p + (ci.top - oldp);
      ExternVar *pev = ci.fd->open_extvar_head;
      for (ExternVar *p = ci.fd->open_extvar_head; p != nullptr;
           p = p->un.next) {
        ASSERT(p->is_open());
        p->value_ref = p->value_ref + (p->value_ref - oldp);
      }
    }

    return m_stack.p + ret_offset;
  }
  void prepare_call(Slot *new_base, size_t param_cnt, FunctionData *fd) {
    ASSERT(fd->shared_data->param_cnt == param_cnt);  //��������ƥ��

    new_base = reserve_stack(new_base, fd->shared_data->max_stack);

    CallInfo new_ci;
    new_ci.is_script_call = true;
    new_ci.fd = fd;
    new_ci.base = new_base;
    new_ci.top = new_base + fd->shared_data->param_cnt -
                 1; /*topΪջ��λ�ã�ջ��(top-base+1)��Ӧ��ȥ1*/
    new_ci.pc = fd->shared_data->instructions->begin();
    list_ci.push(new_ci);
  }
#if (DEBUG_LOG_EXEC_INFO)

  void dbg_print_stack() {
    fprintf(dbg_f, "----------stack---------\n");
    for (const auto &ci : list_ci) {
      fprintf(dbg_f, "CallInfo: ");
      if (ci.is_script_call) {
        fprintf(dbg_f, "FullCall<%p:'%s'@%p>\n", ci.fd,
                ci.fd->shared_data->name->cstr(), ci.fd->shared_data);
      } else {
        fprintf(dbg_f, "NativeCall\n");
      }
      for (Slot *p = ci.base; p <= ci.top; p++) {
        fprintf(dbg_f, "  [%llu:%llu] ", p - m_stack.p, p - ci.base);
        debug_print(dbg_f, *p);
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

  //ִ����ϵ�CallInfo��ֱ��CallTypeΪNativeCall
  void Execute() {
    Exception *exception = nullptr;
  l_begin:
    CallInfo *ci = &list_ci.back();
    if (!ci->is_script_call) return;

#if (DEBUG_LOG_EXEC_INFO)
    dbg_print_stack();
#endif

    byte *&pc = ci->pc;  //ָ������ã��Ա��޸�ʱͬ����CallInfo��
    // topָ��ջ��Ԫ�أ�����ջ��+1��
    // top�����ڱ�ʶgc��Χ��Ӧ��ÿ��ָ��������ٵ���topָ��
    Slot *&top = ci->top;  //ָ������ã��Ա��޸�ʱͬ����CallInfo��
    Slot *&base = ci->base;  //ָ������ã��Ա��޸�ʱͬ����CallInfo��
    Object **kpool = ci->fd->shared_data->kpool->begin();
    Object **inner_func = ci->fd->shared_data->inner_func->begin();
    ExternVar **extern_var =
        reinterpret_cast<ExternVar **>(ci->fd->extvars->begin());
    while (true) {
#if (DEBUG_LOG_EXEC_INFO)
      char buff[64];
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
        // case Opcode::LOAD_THIS:
        //  *++top = ci->this_object;
        //  break;
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
          Array *arr = NewArray(cnt);  //�˺������ܴ���GC����֮ǰtop���ܱ�
          top -= cnt - 1;
          arr->quick_init(top, cnt);
          top[0] = arr;
          break;
        }
        case Opcode::MAKE_ARRAY_0:
          top[1] = NewArray(0);
          ++top;
          break;
        case Opcode::MAKE_DICTIONARY: {
          uint16_t cnt = *(uint16_t *)pc;
          pc += 2;
          Dictionary *tb =
              NewDictionary(cnt);  //�˺������ܴ���GC����֮ǰtop���ܱ�
          top -= cnt * 2 - 1;
          tb->quick_init(top, cnt);
          top[0] = tb;
          break;
        }
        case Opcode::MAKE_DICTIONARY_0:
          top[1] = NewDictionary(4);
          ++top;
          break;
        case Opcode::MAKE_TABLE: {
          TableInfo *ti = TableInfo::cast(
              ci->fd->shared_data->tableinfo->get(*(uint16_t *)pc));
          pc += 2;
          *(top - ti->prop_count() + 1) =
              NewTable(ti, top - ti->prop_count() + 1);
          top = top - ti->prop_count() + 1;
          break;
        }
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

        case Opcode::GET_P:  // obj name
          top[-1] = GetProperty(top[-1], String::cast(top[0]));
          --top;
          break;
        case Opcode::SET_P:  // val obj name
          SetProperty(top[-1], String::cast(top[0]), top[-2]);
          top -= 3;
          break;
        case Opcode::GET_I: {
          top[-1] = GetIndex(top[-1], top[0]);
          --top;
          break;
        }

        case Opcode::SET_I: {  // value obj index
          SetIndex(top[-1], top[0], top[-2]);
          top -= 3;
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
        case Opcode::CALL: {  // func p1 p2 p3 [<- top]
          uint16_t cnt = *(uint16_t *)pc;
          pc += 2;
          ASSERT(ci == &list_ci.back());
          Object *func = *(top - cnt);
          if (func->IsFunctionData()) {
            FunctionData *fd = FunctionData::cast(func);
            top -= cnt;  //��ʱtopָ��Ҫ���õĶ���ͬʱ��λ��Ҳ���ܷ���ֵ
            prepare_call(top + 1, cnt, fd);
            goto l_begin;
          } else if (func->IsNativeObject()) {
            HandleScope hs;
            Parameters params(top - cnt + 1, cnt);
            Object *ret = NativeObject::cast(func)->interface()->invoke(
                NativeObject::cast(func)->data(), params);
            top -= cnt;  //��ʱtopָ��Ҫ���õĶ���ͬʱ��λ��Ҳ���ܷ���ֵ
            *top = ret;
          } else {
            ASSERT(0);
          }
          break;
        }
        case Opcode::RET:
          close_all_extern_var(&ci->fd->open_extvar_head);
          list_ci.pop();
          list_ci.back().top[0] = top[0];  //����ֵ��������һ����õ�ջ��
          goto l_begin;
          break;
        case Opcode::RETNULL:
          close_all_extern_var(&ci->fd->open_extvar_head);
          list_ci.pop();
          list_ci.back().top[0] = nullptr;  //����ֵ��������һ����õ�ջ��
          goto l_begin;
          break;
        case Opcode::CLOSURE: {
          FunctionData *fd = NewFunctionData(
              SharedFunctionData::cast(inner_func[*(uint16_t *)pc]));
          pc += 2;
          *++top = fd;
          for (size_t i = 0; i < fd->shared_data->extvars->length(); i++) {
            const auto &evi = fd->shared_data->extvars->at(i);
            if (!evi.in_stack) {  //�ڴ˺������ⲿ����������
              fd->extvars->set(i, ci->fd->extvars->get(evi.pos));
              continue;
            }

            //�ڴ˺�����ջ��
            //���ڴ򿪵��ⲿ��������������

            ExternVar *p = ci->fd->open_extvar_head;
            Object **stack_value_ref = base + evi.pos;
            while (p != nullptr && p->value_ref != stack_value_ref) {
              p = p->un.next;
            }
            if (p != nullptr &&
                p->value_ref ==
                    stack_value_ref) {  //�ڴ򿪵��ⲿ���������У�ֱ�����
              fd->extvars->set(i, p);
            } else {  //���ڴ򿪵��ⲿ���������У���������ӵ�����
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
        //        AllocFunctionData();  // TODO:����AllocFunctionDataʧ�ܵ����
        //    fd->shared_data = sfd;
        //    *++top = fd;
        //  }
        //  break;
        //}
        case Opcode::FOR_RANGE_BEGIN: {
          uint64_t i = Integer::cast(base[*(uint16_t *)pc])->value();
          uint64_t end = Integer::cast(base[*(uint16_t *)pc + 1])->value();
          uint64_t step = Integer::cast(base[*(uint16_t *)pc + 2])->value();
          if (step > 0) {
            if (i >= end) {
              pc = pc - 1 + *(int32_t *)(pc + 2);
              break;
            }
          } else if (step < 0) {
            if (i <= end) {
              pc = pc - 1 + *(int32_t *)(pc + 2);
              break;
            }
          } else {
            VERIFY(0);
          }
          pc += 6;
          break;
        }
        case Opcode::FOR_RANGE_END: {
          // offset=<uint16_t> pc-1+*(int32_t *)pc+1 -- ѭ��������λ��
          uint16_t offset = *(uint16_t *)(pc + *(int32_t *)pc);
          uint64_t i = Integer::cast(base[offset])->value();
          uint64_t end = Integer::cast(base[offset + 1])->value();
          uint64_t step = Integer::cast(base[offset + 2])->value();
          base[offset] =
              Integer::FromInt64(Integer::cast(base[offset])->value() +
                                 Integer::cast(base[offset + 2])->value());
          pc = pc - 1 + *(int32_t *)pc;
          break;
        }
        default:
          ASSERT(0);
      }
#if (DEBUG_LOG_EXEC_INFO)
      dbg_print_stack();
#endif
    }
    goto l_ececute_exit;
  l_exception:  //�����쳣
    VERIFY(0);

  l_ececute_exit:;
  }

  Object *CallFunction(FunctionData *fd, const Parameters &param) {
    if (fd->shared_data->param_cnt != param.count()) {
      VERIFY(0);  // TODO;
    }
    Slot *base;
    if (list_ci.empty()) {
      base = m_stack.p;
    } else {
      base = list_ci.back().top + 1;
    }
    base = reserve_stack(base, fd->shared_data->max_stack + 1 /*����ֵ*/);

    CallInfo native_ci;
    native_ci.is_script_call = false;
    native_ci.base = base;
    native_ci.top = base;
    base[0] = nullptr;
    list_ci.push(native_ci);

    ++base;
    prepare_call(base, param.count(), fd);
    for (size_t i = 0; i < param.count(); i++) base[i] = param[i];
    Execute();

    ASSERT(!list_ci.back().is_script_call);
    ASSERT(list_ci.back().base == native_ci.base);

    list_ci.pop();
    return *native_ci.base;
  }

 public:
  static ExecuterImpl *Create() {
    ExecuterImpl *p = Allocate<ExecuterImpl>();
    p->m_stack.p = Allocate<Slot>(Config::InitialStackSlotCount);
    p->m_stack.size = Config::InitialStackSlotCount;
    new (&p->list_ci) List<CallInfo>();
    p->m_module = Factory::NewDictionary().ptr();
    p->err = nullptr;
#if (DEBUG_LOG_EXEC_INFO)
    p->dbg_f = fopen(DEBUG_LOG_DIRECTORY "exec_log.txt", "w");
#endif  // _DEBUG
    return p;
  }
  static void Destory(Executer *p) {
    // TODO��ʵ��ExecuterImpl::Destory
  }

 public:
  void TraceStack(GCTracer *gct) {
    gct->Trace(this->m_module);
    gct->Trace(this->err);
    if (list_ci.empty()) return;
    for (const auto &ci : list_ci) {
      if (ci.is_script_call) gct->Trace(ci.fd);
    }
    for (Slot *p = m_stack.p; p < m_stack.p + m_stack.size; p++) {
      gct->Trace(*p);
    }
  }

  void ThrowException(uint64_t id, Handle<String> info) {
    HandleScope hs;
    if (err != nullptr) {
      fprintf(stderr,
              "abort in ThrowException(): another exception throwed before "
              "handling exception.\n");
      fprintf(stderr, "this exception: <%s>'%s'",
              ExceptionTree::GetType(id)->cstr(), info->cstr());
      fprintf(stderr, "previous exception: <%s>'%s'",
              ExceptionTree::GetType(err->id)->cstr(), err->info->cstr());
      abort();
    }
    err = *Factory::NewException(id, info, GetStackTrace());
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
  Handle<String> GetStackTrace() {
    StringBuilder sb;
    for (auto &ci : list_ci) {
      if (!ci.is_script_call) {
        sb.AppendString("at [c++ code]\n");
        continue;
      }
      int cmd_cnt = 0;
      byte *pc = ci.fd->shared_data->instructions->begin();
      while (pc != ci.pc) {
        ASSERT(pc < ci.pc);
        ++cmd_cnt;
        pc += read_bytecode(pc, nullptr);
      }
      sb.AppendFormat(
          "at %s:%d in function '%s'\n", ci.fd->shared_data->filename->cstr(),
          Integer::cast(ci.fd->shared_data->bytecode_line->get(cmd_cnt))
              ->value(),
          ci.fd->shared_data->name->cstr());
    }
    return sb.ToString();
  }
};

Executer *Executer::Create() { return ExecuterImpl::Create(); }

void Executer::Destory(Executer *p) { return ExecuterImpl::Destory(p); }
#define CALL_EXECUTER_IMPL(_f, ...) \
  (((ExecuterImpl *)Global::GetExecuter())->_f(__VA_ARGS__))
void Executer::TraceStack(GCTracer *gct) {
  return CALL_EXECUTER_IMPL(TraceStack, gct);
}
void Executer::ThrowException(uint64_t id, Handle<String> info) {
  return CALL_EXECUTER_IMPL(ThrowException, id, info);
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