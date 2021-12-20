#include "compiler.h"
namespace rapid {
namespace internal {

// constexpr bool __char_equal(const char* s1, const char* s2, size_t p) {
//  return s1[p] == s2[p];
//}
// template <size_t... Idx>
// constexpr bool __strnequal_impl(const char* s1, const char* s2,
//                                std::index_sequence<Idx...>&&) {
//  return (... && __char_equal(s1, s2, Idx));
//}
// template <size_t LEN>
// constexpr bool strnequal(const char* ps, const char (&pkw)[LEN]) {
//  return __strnequal_impl(ps, pkw, std::make_index_sequence<LEN - 1>{});
//}

template <size_t LEN>
constexpr bool strnequal(const char* ps, const char (&pkw)[LEN]) {
  for (size_t i = 0; i < LEN - 1; i++)
    if (ps[i] != pkw[i]) return false;
  return true;
}

struct __char_check_t {
  static constexpr uint8_t Alphabet = 1;
  static constexpr uint8_t Number = 2;
  static constexpr uint8_t ExtraLetter = 4;
  uint8_t _t[128];

 public:
  constexpr __char_check_t() : _t() {
    for (int i = 0; i < 26; i++) {
      _t['a' + i] |= Alphabet;
      _t['A' + i] |= Alphabet;
    }
    for (int i = 0; i < 10; i++) _t['0' + i] |= Number;
    _t['_'] |= ExtraLetter;
    _t['$'] |= ExtraLetter;
  }

 public:
  bool is_allowed_symbol_suffix(char c) const {
    return c > 0 && (_t[c] & (Alphabet | Number | ExtraLetter));
  }
};

static constexpr __char_check_t char_check;
inline bool is_allowed_symbol_suffix(char c) {
  return char_check.is_allowed_symbol_suffix(c);
}

void TokenStream::Step(int step) {
  for (int i = 0; i < step; i++) {
    if (*ps == '\n') {
      ++row;
      col = 0;
    } else {
      ++col;
    }
    ++ps;
  }
}

void TokenStream::InitToken(TokenType type) {
  t.t = type;
  t.row = row;
  t.col = col;
  t.v = Handle<Object>();
}

void TokenStream::ReadTokenIntOrFloat() {
  InitToken(TokenType::KVAL);
  char* pend;
  t.v = Factory::NewInteger(strtoll(ps, &pend, 10));
  if ((*pend == '.' && *(pend + 1)) || *pend == 'e' || *pend == 'E') {
    t.v = Factory::NewFloat(strtod(ps, &pend));
  }
  col += pend - ps;
  ps = pend;
}

void TokenStream::ReadTokenSymbol() {
  InitToken(TokenType::SYMBOL);
  const char* pend = ps + 1;
  while (is_allowed_symbol_suffix(*pend)) ++pend;
  t.v = Factory::NewString(ps, pend - ps);
  col += pend - ps;
  ps = pend;
}

void TokenStream::ReadTokenString() {
  InitToken(TokenType::KVAL);
  Step();
  buffer.clear();
  while (true) {
  l_beginstr:
    char c = *ps;
    switch (c) {
      case '\"':
        Step();
        goto l_endstr;
      case '\r':
      case '\n':
        VERIFY(0);
        break;
      case '\\': {
        Step();
        switch (*ps) {
          case 'r':
            c = '\r';
            break;
          case 'n':
            c = '\n';
            break;
          case 't':
            c = '\t';
            break;
          case 'a':
            c = '\a';
            break;
          case 'b':
            c = '\b';
            break;
          case 'f':
            c = '\f';
            break;
          case 'v':
            c = '\v';
            break;
          case '\\':
            c = '\\';
            break;
          case '\"':
            c = '\"';
            break;
          case '\n':
            Step();
            goto l_beginstr;
            break;
          case '\r': {
            if (ps[1] == '\n') {
              Step(2);
              goto l_beginstr;
            }
            VERIFY(0);
            break;
          }
          default:
            VERIFY(0);
        }
        break;
      }
      default:
        break;
    }
    buffer.push(c);
    Step();
  }
l_endstr:
  t.v = Factory::NewString(buffer.begin(), buffer.size());
}
void TokenStream::ReadToken() {
  if (*ps == '\0') {
    InitToken(TokenType::END);
    return;
  }
l_begin_switch:
  switch (*ps) {
    case '\0':
      InitToken(TokenType::END);
      return;
      break;
    case ' ':
    case '\t':
      Step();
      goto l_begin_switch;
      break;
    case '\r':
      Step();
      goto l_begin_switch;
      break;
    case '\n':
      Step();
      goto l_begin_switch;
      break;

    case '"':
      ReadTokenString();
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      ReadTokenIntOrFloat();
      break;

#define CASE_CTRL_ITERATOR(_t, _s) \
  static_assert(sizeof(_s) == 2);  \
  case _s[0]:                      \
    InitToken(_t);                 \
    Step(1);                       \
    break;

      TT_ITER_CONTROL(CASE_CTRL_ITERATOR)

    case '!':
      if (ps[1] == '=') {
        InitToken(TokenType::NEQ);
        Step(2);
      } else {
        InitToken(TokenType::NOT);
        Step(1);
      }
      break;
    case '%':
      if (ps[1] == '=') {
        InitToken(TokenType::MOD_ASSIGN);
        Step(2);
      } else {
        InitToken(TokenType::MOD);
        Step(1);
      }
      break;
    case '&':
      if (ps[1] == '=') {
        InitToken(TokenType::BAND_ASSIGN);
        Step(2);
      } else {
        InitToken(TokenType::BAND);
        Step(1);
      }
      break;
    case '*':
      if (ps[1] == '=') {
        InitToken(TokenType::MUL_ASSIGN);
        Step(2);
      } else {
        InitToken(TokenType::MUL);
        Step(1);
      }
      break;
    case '+':
      if (ps[1] == '=') {
        InitToken(TokenType::ADD_ASSIGN);
        Step(2);
      } else {
        InitToken(TokenType::ADD);
        Step(1);
      }
      break;
    case '-':
      if (ps[1] == '=') {
        InitToken(TokenType::SUB_ASSIGN);
        Step(2);
      } else {
        InitToken(TokenType::SUB);
        Step(1);
      }
      break;
    case '/':
      if (ps[1] == '=') {
        InitToken(TokenType::FDIV_ASSIGN);
        Step(2);
      } else if (ps[1] == '/') {
        if (ps[2] == '=') {
          InitToken(TokenType::IDIV_ASSIGN);
          Step(3);
        } else {
          InitToken(TokenType::IDIV);
          Step(2);
        }
      } else {
        InitToken(TokenType::FDIV);
        Step(1);
      }
      break;
    case '=':
      if (ps[1] == '=') {
        InitToken(TokenType::EQ);
        Step(2);
      } else {
        InitToken(TokenType::ASSIGN);
        Step(1);
      }
      break;
    case '<':
      if (ps[1] == '=') {
        InitToken(TokenType::LE);
        Step(2);
      } else if (ps[1] == '<') {
        if (ps[2] == '=') {
          InitToken(TokenType::SHL_ASSIGN);
          Step(3);
        } else {
          InitToken(TokenType::SHL);
          Step(2);
        }
      } else {
        InitToken(TokenType::LT);
        Step(1);
      }
      break;

    case '>':
      if (ps[1] == '=') {
        InitToken(TokenType::GE);
        Step(2);
      } else if (ps[1] == '>') {
        if (ps[2] == '=') {
          InitToken(TokenType::SHR_ASSIGN);
          Step(3);
        } else {
          InitToken(TokenType::SHR);
          Step(2);
        }
      } else {
        InitToken(TokenType::GT);
        Step(1);
      }
      break;
    case 'b':
      if (strnequal(ps, "break")) {
        InitToken(TokenType::BREAK);
        Step(5);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'c':
      if (strnequal(ps, "catch")) {
        InitToken(TokenType::CATCH);
        Step(5);
      } else if (strnequal(ps, "const")) {
        InitToken(TokenType::CONST);
        Step(5);
      } else if (strnequal(ps, "continue")) {
        InitToken(TokenType::CONTINUE);
        Step(8);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'e':
      if (strnequal(ps, "else")) {
        InitToken(TokenType::ELSE);
        Step(4);
      } else if (strnequal(ps, "export")) {
        InitToken(TokenType::EXPORT);
        Step(6);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'f':
      if (strnequal(ps, "for")) {
        InitToken(TokenType::FOR);
        Step(3);
      } else if (strnequal(ps, "func")) {
        InitToken(TokenType::FUNC);
        Step(4);
      } else if (strnequal(ps, "false")) {
        InitToken(TokenType::KVAL);
        t.v = Factory::FalseValue();
        Step(5);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'i':
      if (strnequal(ps, "if")) {
        InitToken(TokenType::IF);
        Step(2);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'r':
      if (strnequal(ps, "return")) {
        InitToken(TokenType::RETURN);
        Step(6);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 't':
      if (strnequal(ps, "try")) {
        InitToken(TokenType::TRY);
        Step(3);
      } else if (strnequal(ps, "true")) {
        InitToken(TokenType::KVAL);
        t.v = Factory::TrueValue();
        Step(4);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'v':
      if (strnequal(ps, "var")) {
        InitToken(TokenType::VAR);
        Step(3);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'w':
      if (strnequal(ps, "while")) {
        InitToken(TokenType::WHILE);
        Step(5);
      } else {
        ReadTokenSymbol();
      }
      break;
    case '|':
      if (strnequal(ps, "|")) {
        InitToken(TokenType::BOR);
        Step(1);
      } else if (strnequal(ps, "|=")) {
        InitToken(TokenType::BOR_ASSIGN);
        Step(2);
      }
      break;
    case 'n':
      if (strnequal(ps, "null")) {
        InitToken(TokenType::KVAL);
        t.v = Factory::NullValue();
        Step(4);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'a':
    case 'd':
    case 'g':
    case 'h':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'o':
    case 'p':
    case 'q':
    case 's':
    case 'u':
    case 'x':
    case 'y':
    case 'z':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case '_':
    case '$':
      ReadTokenSymbol();
      break;
    default:
      ASSERT(0);
      break;
  }
}

TokenStream::TokenStream(const char* s)
    : ps(s), row(0), col(0), t({TokenType::END, 0, 0, Factory::NullValue()}) {
  ReadToken();
}

Token& TokenStream::peek() { return t; }

void TokenStream::consume() { ReadToken(); }

//-----------------------------------------------------------------------

void CodeGenerator::VisitExpressionStat(ExpressionStat* p) {
  Visit(p->expr);
  Append(Opcode::POP);
}

void CodeGenerator::VisitLiteral(Literal* p) { LoadK(p->value); }

void CodeGenerator::VisitBlockStat(BlockStat* p) {
  for (auto node : p->stat) {
    Visit(node);
  }
}

void CodeGenerator::VisitIfStat(IfStat* p) {
  Visit(p->cond);
  Codepos jmp_to_else = PrepareJumpIf(false);
  Visit(p->then_stat);
  if (p->else_stat) {
    Codepos jmp_to_end = PrepareJump();
    ApplyJump(jmp_to_else, CurrentPos());
    Visit(p->else_stat);
    ApplyJump(jmp_to_end, CurrentPos());
  } else {
    ApplyJump(jmp_to_else, CurrentPos());
  }
}

void CodeGenerator::VisitLoopStat(LoopStat* p) {
  LoopCtx* upper = loop_ctx;
  LoopCtx* current = AllocLoopCtx();
  loop_ctx = nullptr;  //不允许init出现break和continue
  Visit(p->init);
  Codepos begin = CurrentPos();
  Visit(p->cond);
  Codepos jmp_to_end = PrepareJumpIf(false);
  loop_ctx = current;
  Visit(p->body);
  loop_ctx = nullptr;  //不允许after出现break和continue
  Codepos continue_topos = CurrentPos();
  Visit(p->after);
  Codepos jmp_to_begin = PrepareJump();
  Codepos end = CurrentPos();

  loop_ctx = upper;  //恢复
  ApplyJump(jmp_to_begin, begin);
  ApplyJump(jmp_to_end, end);
  for (auto pos : current->break_pos) {
    ApplyJump(pos, end);
  }
  for (auto pos : current->continue_pos) {
    ApplyJump(pos, continue_topos);
  }
}

void CodeGenerator::VisitVarDecl(VarDecl* p) {
  for (auto decl : p->decl) {
    uint16_t pos = AddVar(decl.name);
    if (decl.init) {
      Visit(decl.init);
      Append(Opcode::STOREL);
      Append((uint16_t)pos);
    }
  }
}

void CodeGenerator::VisitFuncDecl(FuncDecl* p) {
  FunctionCtx* fc = AllocFunctionCtx();
  fc->name = p->name;
  fc->param_cnt = p->param.size();
  fc->top = fc->max_stack = p->param.size();
  for (auto& param : p->param) {
    fc->var.push(VarCtx{param});
  }
  if (ctx == nullptr) {
    ctx = fc;
    Visit(p->body);
    if (ctx->cmd.size() == 0 || ctx->cmd.back() != (uint8_t)Opcode::RET) {
      Append(Opcode::RETNULL);
    }
  } else {
    ctx->inner_func.push(fc);
    FunctionCtx* upper = ctx;
    ctx = fc;
    Visit(p->body);
    if (ctx->cmd.size() == 0 || ctx->cmd.back() != (uint8_t)Opcode::RET) {
      Append(Opcode::RETNULL);
    }
    ctx = upper;
  }
}

void CodeGenerator::VisitReturnStat(ReturnStat* p) {
  Visit(p->expr);
  Append(Opcode::RET);
}

void CodeGenerator::VisitBreakStat(BreakStat* p) {
  loop_ctx->break_pos.push(CurrentPos());
  Append(Opcode::JMP);
  Append((uint16_t)0);
}

void CodeGenerator::VisitContinueStat(ContinueStat* p) {
  loop_ctx->continue_pos.push(CurrentPos());
  Append(Opcode::JMP);
  Append((uint16_t)0);
}

void CodeGenerator::VisitVarExpr(VarExpr* p) {
  if (String::Equal(*p->name, *ctx->name)) {
    Append(Opcode::CLOSURE_SELF);
    return;
  }
  bool b = LoadL(p->name);
  if (b) return;
  b = Closure(p->name);
  VERIFY(b);
  // TODO: extvar
}

void CodeGenerator::VisitMemberExpr(MemberExpr* p) {
  Visit(p->target);
  LoadK(p->name);
  Append(Opcode::GET_M);
}

void CodeGenerator::VisitIndexExpr(IndexExpr* p) {
  Visit(p->target);
  Visit(p->index);
  Append(Opcode::GET_M);
}

void CodeGenerator::VisitUnaryExpr(UnaryExpr* p) {
  Visit(p->expr);
  switch (p->opt) {
    case TokenType::ADD:
      Append(Opcode::ACT);
      break;
    case TokenType::SUB:
      Append(Opcode::NEG);
      break;
    case TokenType::NOT:
      Append(Opcode::NOT);
      break;
    case TokenType::BNOT:
      Append(Opcode::BNOT);
      break;
    default:
      ASSERT(0);
  }
}

void CodeGenerator::VisitBinaryExpr(BinaryExpr* p) {
  Visit(p->left);
  Visit(p->right);
  switch (p->opt) {
    case TokenType::ADD:
      Append(Opcode::ADD);
      break;
    case TokenType::SUB:
      Append(Opcode::SUB);
      break;
    case TokenType::MUL:
      Append(Opcode::MUL);
      break;
    case TokenType::IDIV:
      Append(Opcode::IDIV);
      break;
    case TokenType::FDIV:
      Append(Opcode::FDIV);
      break;
    case TokenType::MOD:
      Append(Opcode::MOD);
      break;
    case TokenType::AND:
      Append(Opcode::AND);
      break;
    case TokenType::OR:
      Append(Opcode::OR);
      break;
    case TokenType::BAND:
      Append(Opcode::BAND);
      break;
    case TokenType::BOR:
      Append(Opcode::BOR);
      break;
    case TokenType::BXOR:
      Append(Opcode::BXOR);
      break;
    case TokenType::SHL:
      Append(Opcode::SHL);
      break;
    case TokenType::SHR:
      Append(Opcode::SHR);
      break;
    case TokenType::LT:
      Append(Opcode::LT);
      break;
    case TokenType::GT:
      Append(Opcode::GT);
      break;
    case TokenType::LE:
      Append(Opcode::LE);
      break;
    case TokenType::GE:
      Append(Opcode::GE);
      break;
    case TokenType::EQ:
      Append(Opcode::EQ);
      break;
    case TokenType::NEQ:
      Append(Opcode::NEQ);
      break;
    default:
      ASSERT(0);
  }
}

void CodeGenerator::VisitAssignExpr(AssignExpr* p) {
  if (p->left->type == AstNodeType::VarExpr) {
    Visit(p->right);
    Append(Opcode::STOREL);
    Append(FindVar(((VarExpr*)p->left)->name));
  } else if (p->left->type == AstNodeType::MemberExpr) {
    Visit(((MemberExpr*)p->left)->target);
    Visit(p->right);
    Append(Opcode::SET_M);
    Append(FindConst(((MemberExpr*)p->left)->name));
  } else if (p->left->type == AstNodeType::IndexExpr) {
    Visit(((IndexExpr*)p->left)->target);
    Visit(((IndexExpr*)p->left)->index);
    Visit(p->right);
    Append(Opcode::SET_M);
  } else {
    VERIFY(0);
  }
}

void CodeGenerator::VisitCallExpr(CallExpr* p) {
  Visit(p->callee);
  for (auto node : p->params) {
    Visit(node);
  }
  Append(Opcode::CALL);
  Append((uint16_t)p->params.size());
}

}  // namespace internal
}  // namespace rapid