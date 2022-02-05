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
// constexpr bool whole_string_nequal(const char* ps, const char (&pkw)[LEN]) {
//  return __strnequal_impl(ps, pkw, std::make_index_sequence<LEN - 1>{});
//}

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
template <size_t LEN>
constexpr bool whole_string_nequal(const char *ps, const char (&pkw)[LEN]) {
  if (is_allowed_symbol_suffix(ps[LEN-1])) return false;
  for (size_t i = 0; i < LEN - 1; i++)
    if (ps[i] != pkw[i]) return false;
  return true;
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
  char *pend;
  t.v = Factory::NewInteger(strtoll(ps, &pend, 10));
  if ((*pend == '.' && *(pend + 1)) || *pend == 'e' || *pend == 'E') {
    t.v = Factory::NewFloat(strtod(ps, &pend));
  }
  col += pend - ps;
  ps = pend;
}

void TokenStream::ReadTokenSymbol() {
  InitToken(TokenType::SYMBOL);
  const char *pend = ps + 1;
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
        VERIFY(0);  // TODO
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
      if (ps[1] == '*') {
        Step(2);
        while (!(ps[0] == '*' && ps[1] == '/')) {
          Step(1);
        }
        Step(2);
        goto l_begin_switch;
      }
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
    case '|':
      if (whole_string_nequal(ps, "|")) {
        InitToken(TokenType::BOR);
        Step(1);
      } else if (whole_string_nequal(ps, "|=")) {
        InitToken(TokenType::BOR_ASSIGN);
        Step(2);
      }
      break;

    case 'b':
      if (whole_string_nequal(ps, "break")) {
        InitToken(TokenType::BREAK);
        Step(5);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'c':
      if (whole_string_nequal(ps, "catch")) {
        InitToken(TokenType::CATCH);
        Step(5);
      } else if (whole_string_nequal(ps, "const")) {
        InitToken(TokenType::CONST);
        Step(5);
      } else if (whole_string_nequal(ps, "continue")) {
        InitToken(TokenType::CONTINUE);
        Step(8);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'e':
      if (whole_string_nequal(ps, "else")) {
        InitToken(TokenType::ELSE);
        Step(4);
      } else if (whole_string_nequal(ps, "export")) {
        InitToken(TokenType::EXPORT);
        Step(6);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'f':
      if (whole_string_nequal(ps, "for")) {
        InitToken(TokenType::FOR);
        Step(3);
      } else if (whole_string_nequal(ps, "func")) {
        InitToken(TokenType::FUNC);
        Step(4);
      } else if (whole_string_nequal(ps, "false")) {
        InitToken(TokenType::KVAL);
        t.v = Factory::FalseValue();
        Step(5);
      } else if (whole_string_nequal(ps, "finally")) {
        InitToken(TokenType::FINALLY);
        Step(7);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'i':
      if (whole_string_nequal(ps, "if")) {
        InitToken(TokenType::IF);
        Step(2);
      } else if (whole_string_nequal(ps, "import")) {
        InitToken(TokenType::IMPORT);
        Step(6);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'r':
      if (whole_string_nequal(ps, "return")) {
        InitToken(TokenType::RETURN);
        Step(6);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 't':
      if (whole_string_nequal(ps, "try")) {
        InitToken(TokenType::TRY);
        Step(3);
      } else if (whole_string_nequal(ps, "true")) {
        InitToken(TokenType::KVAL);
        t.v = Factory::TrueValue();
        Step(4);
      } else if (whole_string_nequal(ps, "this")) {
        InitToken(TokenType::THIS);
        Step(4);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'v':
      if (whole_string_nequal(ps, "var")) {
        InitToken(TokenType::VAR);
        Step(3);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'w':
      if (whole_string_nequal(ps, "while")) {
        InitToken(TokenType::WHILE);
        Step(5);
      } else {
        ReadTokenSymbol();
      }
      break;

    case 'n':
      if (whole_string_nequal(ps, "null")) {
        InitToken(TokenType::KVAL);
        t.v = Factory::NullValue();
        Step(4);
      } else {
        ReadTokenSymbol();
      }
      break;
    case 'p':
      if (whole_string_nequal(ps, "params")) {
        InitToken(TokenType::PARAMS);
        Step(6);
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

TokenStream::TokenStream(const char *s)
    : ps(s), row(0), col(0), t({TokenType::END, 0, 0, Factory::NullValue()}) {
  ReadToken();
}

Token &TokenStream::peek() { return t; }

void TokenStream::consume() { ReadToken(); }

//-----------------------------------------------------------------------

void CodeGenerator::VisitWithScope(AstNode *node) {
  EnterScope();
  Visit(node);
  LeaveScope();
}

void CodeGenerator::VisitExpressionStat(ExpressionStat *p) {
  if (p->expr->type == AstNodeType::AssignExpr) {
    VisitAssignExpr((AssignExpr *)p->expr, true);
    return;
  }
  Visit(p->expr);
  AppendOp(Opcode::POP);
  pop();
}

void CodeGenerator::VisitLiteral(Literal *p) { LoadK(p->value); }

void CodeGenerator::VisitBlockStat(BlockStat *p) {
  EnterScope();
  for (auto node : p->stat) {
    Visit(node);
    if (node->type == AstNodeType::ReturnStat) {
      LeaveScope(false);//无需清理，RET指令会做清理工作
      return;
    }
  }
  LeaveScope();
}

void CodeGenerator::VisitIfStat(IfStat *p) {
  Visit(p->cond);
  Codepos jmp_to_else = PrepareJumpIf(false);
  VisitWithScope(p->then_stat);
  if (p->else_stat) {
    Codepos jmp_to_end = PrepareJump();
    ApplyJump(jmp_to_else, CurrentPos());
    VisitWithScope(p->else_stat);
    ApplyJump(jmp_to_end, CurrentPos());
  } else {
    ApplyJump(jmp_to_else, CurrentPos());
  }
}

void CodeGenerator::VisitLoopStat(LoopStat *p) {
  EnterScope();  //整个循环为一个scope，以便for定义循环变量
  LoopCtx *upper = loop_ctx;

  LoopCtx *current = AllocLoopCtx();

  loop_ctx = nullptr;  //不允许init出现break和continue

  Visit(p->init);
  Codepos begin = CurrentPos();
  Visit(p->cond);
  Codepos jmp_to_end = PrepareJumpIf(false);
  loop_ctx = current;

  EnterScope();  // body单独定义一个Scope，

  Visit(p->body);
  loop_ctx = nullptr;  //不允许after出现break和continue
  Codepos continue_topos = CurrentPos();
  Visit(p->after);

  LeaveScope();  // body LeaveScope

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

  LeaveScope();  //每次循环都应该LeaveScope
}

void CodeGenerator::VisitVarDecl(VarDecl *p) {
  for (auto decl : p->decl) {
    if (decl.init) {
      Visit(decl.init);
      // AppendOp(Opcode::PUSH); -- 不需要
    } else {
      AppendOp(Opcode::PUSH_NULL);
      push();
    }
    AddVar(decl.name);
  }
}

void CodeGenerator::VisitFuncDecl(FuncDecl *p) {
  ASSERT(ctx != nullptr);
  FunctionCtx *fc = AllocFunctionCtx();
  fc->name = p->name;
  fc->outer_func = ctx;
  fc->param_cnt = p->param.size();
  fc->top = fc->max_stack = p->param.size();
  for (size_t i = 0; i < p->param.size(); i++) {
    VarCtx vc;
    vc.name = p->param[i];
    vc.slot_id = (uint16_t)i;
    fc->var.push(vc);
    fc->allvar.push(vc);
  }
  uint16_t fdp = ctx->inner_func.size();
  ctx->inner_func.push(fc);
  FunctionCtx *upper = ctx;
  ctx = fc;
  Visit(p->body);
  if (ctx->cmd.size() == 0 || (ctx->cmd.back() != (uint8_t)Opcode::RET &&
                               ctx->cmd.back() != (uint8_t)Opcode::RETNULL )) {
    AppendOp(Opcode::RETNULL);
  }
  ctx = upper;
  Closure(fdp);
  AddVar(p->name);
  // TODO：整理trycatch块，把catch代码都放到函数末尾
}

void CodeGenerator::VisitReturnStat(ReturnStat *p) {
  if (p->expr == nullptr) {
    AppendOp(Opcode::RETNULL);
    return;
  }
  Visit(p->expr);
  AppendOp(Opcode::RET);
  pop();
}

void CodeGenerator::VisitBreakStat(BreakStat *p) {
  loop_ctx->break_pos.push(CurrentPos());
  AppendOp(Opcode::JMP);
  AppendU16((uint16_t)0);
}

void CodeGenerator::VisitContinueStat(ContinueStat *p) {
  loop_ctx->continue_pos.push(CurrentPos());
  AppendOp(Opcode::JMP);
  AppendU16((uint16_t)0);
}

void CodeGenerator::VisitTryCatchStat(TryCatchStat *p) {
  ASSERT(0);  //先解决闭包，然后catch块直接编译成一个闭包函数
  Codepos try_begin = CurrentPos();
  VisitBlockStat(p->try_);  // try本身带scope
  Codepos try_end = CurrentPos();

  EnterScope();
  AddVar(p->err_var_name);
  Visit(p->catch_);
  LeaveScope();
  Codepos catch_end = CurrentPos();

  if (p->finally_ != nullptr)
    VisitBlockStat(p->finally_);  // finally本身带scope

  TryCatchCtx tcc;
  tcc.try_begin = (uint32_t)try_begin;
  tcc.try_end = (uint32_t)try_end;
  tcc.catch_begin = (uint32_t)try_end;
  tcc.catch_end = (uint32_t)catch_end;
  ctx->try_catch.push(tcc);
}

void CodeGenerator::VisitVarExpr(VarExpr *p) {
  if (LoadL(p->name)) return;
  if (LoadE(p->name)) return;
  error_symbol_notfound(p);
  ASSERT(0);
}

void CodeGenerator::VisitMemberExpr(MemberExpr *p) {
  Visit(p->target);
  LoadK(p->name);
  AppendOp(Opcode::GET_P);
  pop();
}

void CodeGenerator::VisitIndexExpr(IndexExpr *p) {
  Visit(p->target);
  Visit(p->index);
  AppendOp(Opcode::GET_I);
  pop();
}

void CodeGenerator::VisitUnaryExpr(UnaryExpr *p) {
  Visit(p->expr);
  switch (p->opt) {
    case TokenType::ADD:
      AppendOp(Opcode::ACT);
      break;
    case TokenType::SUB:
      AppendOp(Opcode::NEG);
      break;
    case TokenType::NOT:
      AppendOp(Opcode::NOT);
      break;
    case TokenType::BNOT:
      AppendOp(Opcode::BNOT);
      break;
    default:
      ASSERT(0);
  }
}

void CodeGenerator::VisitBinaryExpr(BinaryExpr *p) {
  if (p->left->type != AstNodeType::VarExpr ||
      p->right->type != AstNodeType::VarExpr) {
    if (p->opt == TokenType::AND) {  // AND短路
      Visit(p->left);
      Codepos j1 = PrepareJumpIf(false);

      Visit(p->right);
      Codepos j2 = PrepareJumpIf(false);

      LoadK(Factory::TrueValue());
      Codepos j3 = PrepareJumpIf(false);

      ApplyJump(j1, CurrentPos());
      ApplyJump(j2, CurrentPos());
      LoadK(Factory::FalseValue());
      ApplyJump(j3, CurrentPos());
      return;
    } else if (p->opt == TokenType::OR) {  // OR短路
      Visit(p->left);
      Codepos j1 = PrepareJumpIf(true);

      Visit(p->right);
      Codepos j2 = PrepareJumpIf(true);

      LoadK(Factory::FalseValue());
      Codepos j3 = PrepareJumpIf(true);

      ApplyJump(j1, CurrentPos());
      ApplyJump(j2, CurrentPos());
      LoadK(Factory::TrueValue());
      ApplyJump(j3, CurrentPos());
      return;
    }
  }
  Visit(p->left);
  Visit(p->right);
  switch (p->opt) {
    case TokenType::ADD:
      AppendOp(Opcode::ADD);
      break;
    case TokenType::SUB:
      AppendOp(Opcode::SUB);
      break;
    case TokenType::MUL:
      AppendOp(Opcode::MUL);
      break;
    case TokenType::IDIV:
      AppendOp(Opcode::IDIV);
      break;
    case TokenType::FDIV:
      AppendOp(Opcode::FDIV);
      break;
    case TokenType::MOD:
      AppendOp(Opcode::MOD);
      break;
    case TokenType::AND:
      AppendOp(Opcode::AND);
      break;
    case TokenType::OR:
      AppendOp(Opcode::OR);
      break;
    case TokenType::BAND:
      AppendOp(Opcode::BAND);
      break;
    case TokenType::BOR:
      AppendOp(Opcode::BOR);
      break;
    case TokenType::BXOR:
      AppendOp(Opcode::BXOR);
      break;
    case TokenType::SHL:
      AppendOp(Opcode::SHL);
      break;
    case TokenType::SHR:
      AppendOp(Opcode::SHR);
      break;
    case TokenType::LT:
      AppendOp(Opcode::LT);
      break;
    case TokenType::GT:
      AppendOp(Opcode::GT);
      break;
    case TokenType::LE:
      AppendOp(Opcode::LE);
      break;
    case TokenType::GE:
      AppendOp(Opcode::GE);
      break;
    case TokenType::EQ:
      AppendOp(Opcode::EQ);
      break;
    case TokenType::NEQ:
      AppendOp(Opcode::NEQ);
      break;
    default:
      ASSERT(0);
  }
  pop();
}

void CodeGenerator::VisitAssignExpr(AssignExpr *p) {
  VisitAssignExpr(p, false);
}

void CodeGenerator::VisitCallExpr(CallExpr *p) {
  if (p->callee->type == AstNodeType::ImportExpr) {  // TODO
    if (p->params.size() != 1) {
      error_illegal_use(p->row, p->col, "import");
    }
    Visit(p->params[0]);
    AppendOp(Opcode::IMPORT);
  } else if (p->callee->type ==
             AstNodeType::MemberExpr) {  //对成员函数的调用，生成THIS_CALL
    Visit(((MemberExpr *)p->callee)->target);
    LoadK(((MemberExpr *)p->callee)->name);
    for (auto node : p->params) {
      Visit(node);
    }
    ThisCall((uint16_t)p->params.size());
  } else {
    Visit(p->callee);
    for (auto node : p->params) {
      Visit(node);
    }
    Call((uint16_t)p->params.size());
  }
}

void CodeGenerator::VisitAssignExpr(AssignExpr *p, bool from_expr_stat) {
  if (p->opt == TokenType::ASSIGN) {
    Visit(p->right);
  } else {
    Visit(p->left);
    Visit(p->right);
    switch (p->opt) {
      case TokenType::ADD_ASSIGN:
        AppendOp(Opcode::ADD);
        break;
      case TokenType::SUB_ASSIGN:
        AppendOp(Opcode::SUB);
        break;
      case TokenType::MUL_ASSIGN:
        AppendOp(Opcode::MUL);
        break;
      case TokenType::IDIV_ASSIGN:
        AppendOp(Opcode::IDIV);
        break;
      case TokenType::FDIV_ASSIGN:
        AppendOp(Opcode::FDIV);
        break;
      case TokenType::MOD_ASSIGN:
        AppendOp(Opcode::MOD);
        break;
      case TokenType::BAND_ASSIGN:
        AppendOp(Opcode::BAND);
        break;
      case TokenType::BOR_ASSIGN:
        AppendOp(Opcode::BOR);
        break;
      case TokenType::BNOT_ASSIGN:
        AppendOp(Opcode::BNOT);
        break;
      case TokenType::BXOR_ASSIGN:
        AppendOp(Opcode::BXOR);
        break;
      case TokenType::SHL_ASSIGN:
        AppendOp(Opcode::SHL);
        break;
      case TokenType::SHR_ASSIGN:
        AppendOp(Opcode::SHR);
        break;
      default:
        ASSERT(0);
    }
    pop();
  }

  if (!from_expr_stat) {
    AppendOp(Opcode::COPY);
    push();
  }

  if (p->left->type == AstNodeType::VarExpr) {
    Handle<String> name = ((VarExpr *)p->left)->name;
    uint16_t pos;
    if ((pos = FindVar(name)) != invalid_pos) {
      StoreL(pos);
    } else if ((pos = FindExternVar(name)) != invalid_pos) {
      StoreE(pos);
    } else {
      error_symbol_notfound((VarExpr *)p->left);
      ASSERT(0);
    }
  } else if (p->left->type == AstNodeType::MemberExpr) {
    Visit(((MemberExpr *)p->left)->target);
    LoadK(((MemberExpr *)p->left)->name);
    AppendOp(Opcode::SET_P);
    pop(2);
  } else if (p->left->type == AstNodeType::IndexExpr) {
    Visit(((IndexExpr *)p->left)->target);
    Visit(((IndexExpr *)p->left)->index);
    AppendOp(Opcode::SET_I);
    pop(2);
  } else {
    ASSERT(0);  //在parser中应报告此类错误
  }
}

void CodeGenerator::VisitThisExpr(ThisExpr *p) {
  AppendOp(Opcode::LOAD_THIS);
  push();
}

void CodeGenerator::VisitParamsExpr(ParamsExpr *p) {
  AppendOp(Opcode::LOAD_PARAMS);
  push();
}

void CodeGenerator::VisitImportExpr(ImportExpr *p) {
  error_illegal_use(p->row, p->col, "import");
}

void CodeGenerator::VisitArrayExpr(ArrayExpr *p) {
  if (p->params.size() == 0) {
    AppendOp(Opcode::MAKE_ARRAY_0);
    push();
  } else {
    for (size_t i = 0; i < p->params.size(); i++) {
      Visit(p->params[i]);
    }
    AppendOp(Opcode::MAKE_ARRAY);
    AppendU16((uint16_t)p->params.size());
    pop(p->params.size() - 1);
  }
}

void CodeGenerator::VisitTableExpr(TableExpr *p) {
  if (p->params.size() == 0) {
    AppendOp(Opcode::MAKE_TABLE_0);
    push();
  } else {
    for (size_t i = 0; i < p->params.size(); i++) {
      LoadK(p->params[i].key);
      Visit(p->params[i].value);
    }
    AppendOp(Opcode::MAKE_TABLE);
    AppendU16((uint16_t)p->params.size());
    pop(p->params.size() * 2 - 1);
  }
}
/*
生成for range的代码

for range(var name:end)
for range(var name:begin,end)
for range(var name:begin,end,step)

*/
void CodeGenerator::VisitForRangeStat(ForRangeStat *p) {
  LoopCtx *upper = loop_ctx;
  LoopCtx *current = AllocLoopCtx();
  loop_ctx = current;

  EnterScope();
  if (p->begin != nullptr) {
    Visit(p->begin);
  } else {
    LoadK(Factory::NewInteger(0));
  }
  AddVar(p->loop_var_name);//i=begin
  ASSERT(p->end != nullptr);
  Visit(p->end);
  AddVar(Factory::NewString("(range end)"));
  if (p->step != nullptr) {
    Visit(p->step);
  } else {
    LoadK(Factory::NewInteger(1));
  }
  AddVar(Factory::NewString("(range step)"));
  Codepos for_range_begin = CurrentPos();
  ForRangeBegin(p->loop_var_name);
  Visit(p->body);
  Codepos for_range_end = CurrentPos();
  ForRangeEnd(for_range_begin);
  Codepos for_range_after_end = CurrentPos();
  LeaveScope();


  loop_ctx = upper;  //恢复
  for (auto pos : current->break_pos) {
    ApplyJump(pos, for_range_after_end);
  }
  for (auto pos : current->continue_pos) {
    ApplyJump(pos, for_range_end);
  }
}

}  // namespace internal
}  // namespace rapid