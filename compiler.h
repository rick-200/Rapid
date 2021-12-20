/*

            Parser --> ByteCodeGenerator --> ByteCode
              ^
              |
-----------------------
| Code --> TokenStream|
-----------------------


Complete Syntax:

block = "{" {stat} "}"

stat = break

exp =

binop = ...
unop = ...

*/

#pragma once
#include "allocation.h"
#include "ast.h"
#include "bytecode.h"
#include "factory.h"
#include "list.h"
#include "preprocessors.h"

namespace rapid {
namespace internal {

class TokenStream : public Malloced {
  const char* ps;
  uint32_t row, col;
  Token t;
  List<char> buffer;

 private:
  void Step(int step = 1);
  void InitToken(TokenType type);
  void ReadTokenIntOrFloat();
  void ReadTokenSymbol();
  void ReadTokenString();
  void ReadToken();

 public:
  TokenStream(const char* s);
  Token& peek();
  void consume();
};
inline bool basic_obj_equal(Object* a, Object* b) {
  if (a->IsInteger()) {
    return b->IsInteger() &&
           Integer::cast(a)->value() == Integer::cast(b)->value();
  } else if (a->IsFloat()) {
    return b->IsFloat() && Float::cast(a)->value() == Float::cast(b)->value();
  } else if (a->IsString()) {
    return b->IsString() && String::Equal(String::cast(a), String::cast(b));
  } else if (a->IsBool()) {
    return b->IsBool() && a->IsTrue() == b->IsTrue();
  } else {
    ASSERT(0);
  }
}
// enum class CodePos {};
// class ByteCodeGenerator {
//  ByteCodeGenerator* outer_func;
//  Handle<String> name;
//  List<uint32_t> cmds;
//  Handle<Array> kpool, vars, extvars;
//  uint16_t max_stack;
//  uint16_t top;
//  List<ByteCodeGenerator*> inner_func;
//
// private:
//  uint16_t FindVar(Handle<String> name) {
//    for (size_t i = 0; i < vars->length(); i++) {
//      if (String::Equal(VarData::cast(vars->get(i))->name, *name))
//        return (uint16_t)i;
//    }
//    return -1;
//  }
//  uint16_t FindConst(Handle<Object> val) {
//    for (size_t i = 0; i < kpool->length(); i++) {
//      if (basic_obj_equal(kpool->get(i), *val)) return (uint16_t)i;
//    }
//    return -1;
//  }
//
// private:
//  void AppendIABC(Opcode op, Oprand a, Oprand b, Oprand c) {
//    cmds.push(EncodeIABC(op, a, b, c));
//  }
//  void AppendIAB(Opcode op, Oprand a, Oprand b) {
//    cmds.push(EncodeIAB(op, a, b));
//  }
//  void AppendIAx(Opcode op, uint64_t ax) { cmds.push(EncodeIAx(op, ax)); }
//
// private:
//  uint16_t AllocRegister() {
//    uint16_t ret = top++;
//    if (top > max_stack) max_stack = top;
//    return ret;
//  }
//
// public:
//  ByteCodeGenerator(Handle<String> name, ByteCodeGenerator* outer = nullptr) {
//    this->name = name;
//    outer_func = outer;
//    max_stack = 0;
//    top = 0;
//    kpool = Factory::NewArray();
//    vars = Factory::NewArray();
//    extvars = Factory::NewArray();
//  }
//
// public:
//  void ClearStack() { top = vars->length(); }
//  void DefVar(Handle<String> name) {
//    ASSERT(vars->length() == top);
//    Handle<VarData> vd = Factory::NewVarData();
//    vd->name = *name;
//    vars->push(*vd);
//    top = vars->length();
//    if (top > max_stack) max_stack = top;
//  }
//  void LoadL(Handle<String> name) {
//    uint16_t vp = FindVar(name);
//    uint16_t rp = AllocRegister();
//    AppendIAB(Opcode::LOADL, OPRAND_R(rp), OPRAND_R(vp));
//  }
//  void StoreL(Handle<String> name) {
//    uint16_t vp = FindVar(name);
//    AppendIAB(Opcode::STOREL, OPRAND_R(vp), OPRAND_R(top - 1));
//    --top;
//    ASSERT(top > vars->length());
//  }
//  void Assign() {
//    Cmd cmd = cmds.back();
//    if (GetOP(cmd) == Opcode::STOREL) {
//      cmds.pop();
//      ((IAB*)&cmd)->a;
//    } else {
//      VERIFY(0);
//    }
//    //AppendIAB(Opcode::COPYR, OPRAND_R())
//  }
//  void OpAssign(Opcode op) {
//    // TODO
//  }
//  void LoadK(Handle<Object> val) {
//    uint16_t kp = FindConst(val);
//    uint16_t rp = AllocRegister();
//    AppendIAB(Opcode::LOADK, OPRAND_R(rp), OPRAND_K(kp));
//  }
//  void BinaryOp(Opcode op) {
//    AppendIABC(op, OPRAND_R(top - 2), OPRAND_R(top - 1), OPRAND_R(top - 1));
//    --top;
//    ASSERT(top > vars->length());
//  }
//  void UnaryOp(Opcode op) {
//    AppendIAB(op, OPRAND_R(top - 1), OPRAND_R(top - 1));
//    ASSERT(top > vars->length());
//  }
//  void Add() { BinaryOp(Opcode::ADD); }
//  void Sub() { BinaryOp(Opcode::SUB); }
//  void Mul() { BinaryOp(Opcode::MUL); }
//  void IDiv() { BinaryOp(Opcode::IDIV); }
//  void FDiv() { BinaryOp(Opcode::FDIV); }
//  void Mod() { BinaryOp(Opcode::MOD); }
//  CodePos CurrentPos() { return static_cast<CodePos>(cmds.size()); }
//  CodePos PrepareJump() {
//    uint32_t pos = cmds.size();
//    cmds.push(placeholder_jump);
//    return static_cast<CodePos>(pos);
//  }
//  void Jump(CodePos from, CodePos to) {
//    uint32_t from_p = static_cast<uint32_t>(from);
//    uint32_t to_p = static_cast<uint32_t>(to);
//    cmds[from_p] = EncodeIAx(Opcode::JMP, to_p);
//  }
//  void JumpIf(CodePos from, CodePos to, bool cond) {
//    uint32_t from_p = static_cast<uint32_t>(from);
//    uint32_t to_p = static_cast<uint32_t>(to);
//    cmds[from_p] = EncodeIAx(cond ? Opcode::JMP_T : Opcode::JMP_F, to_p);
//  }
//  void Call(uint32_t param_cnt) {}
//  void GetIndex() {}
//  uint32_t FindInnerFunc(ByteCodeGenerator* bcg) {
//    for (size_t i = 0; i < inner_func.size(); i++) {
//      if (inner_func[i] == bcg) return i;
//    }
//    return -1;
//  }
//  ByteCodeGenerator* CreateInnerFunc(Handle<String> name) {  //
//  TODO:生成Closure
//    ByteCodeGenerator* p = new ByteCodeGenerator(name, this);
//    inner_func.push(p);
//    return p;
//  }
//  ByteCodeGenerator* CreateInnerAnonymousFunc() {
//    size_t x = inner_func.size();
//    char buff[40];
//    sprintf_s(buff, "anonymous<%llu>", x);
//    return CreateInnerFunc(Factory::NewString(buff));
//  }
//  void DefParam(Handle<String> name) {
//    ASSERT(cmds.size() == 0);
//    DefVar(name);
//  }
//};

class BinopParserParam {
  uint64_t val;
  static_assert((int)TokenType::__SIZE < 64);

 public:
  constexpr BinopParserParam(const std::initializer_list<TokenType>& list)
      : val(0) {
    for (auto t : list) {
      val |= (1ULL << (int)t);
    }
  }
  bool test(TokenType t) const { return (val >> (int)t) & 1; }
};
// constexpr BinopParserParam bpp = {TokenType::ADD};
class Parser {
  TokenStream* m_ts;

 private:
 private:
#define TK m_ts->peek()
#define CONSUME m_ts->consume()
#define REQUIRE(_t)          \
  if (TK.t != _t) VERIFY(0); \
  CONSUME

  Expression* ParseFactor() {
    switch (TK.t) {
      case TokenType::SYMBOL: {
        VarExpr* p = AllocVarExpr();
        p->name = Handle<String>::cast(TK.v);
        CONSUME;
        return p;
      }
      case TokenType::KVAL: {
        Literal* p = AllocLiteral();
        p->value = TK.v;
        CONSUME;
        return p;
      }
      case TokenType::BK_SL: {  //(
        CONSUME;
        Expression* p = ParseExpression();
        REQUIRE(TokenType::BK_SR);
        return p;
      }
    }
    return nullptr;
  }
  Expression* ParseUnary() {
    switch (TK.t) {
      case TokenType::ADD:
      case TokenType::SUB:
      case TokenType::NOT:
      case TokenType::BNOT: {  //右结合
        UnaryExpr* p = AllocUnaryExpr();
        p->opt = TK.t;
        CONSUME;
        p->expr = ParseUnary();
        VERIFY(p->expr);
        return p;
      }
    }
    return ParseFactor();
  }
  Expression* ParseInvokeOrGetMemberOrGetIndex() {
    Expression* upper = ParseUnary();
    if (upper == nullptr) return nullptr;
    while (true) {
      switch (TK.t) {
        case TokenType::BK_SL: {  // (
          CallExpr* p = AllocCallExpr();
          p->callee = upper;
          CONSUME;
          if (TK.t != TokenType::BK_SR) {
            while (true) {
              Expression* param = ParseExpression();
              VERIFY(param);
              p->params.push(param);
              if (TK.t != TokenType::COMMA) break;
              CONSUME;
            }
          }
          REQUIRE(TokenType::BK_SR);
          upper = p;
          break;
        }
        case TokenType::BK_ML: {  // [
          CONSUME;
          IndexExpr* p = AllocIndexExpr();
          p->target = upper;
          p->index = ParseExpression();
          REQUIRE(TokenType::BK_MR);
          upper = p;
          break;
        }
        case TokenType::DOT: {  // .
          CONSUME;
          MemberExpr* p = AllocMemberExpr();
          p->target = upper;
          if (TK.t != TokenType::SYMBOL) VERIFY(0);
          p->name = Handle<String>::cast(TK.v);
          CONSUME;
          upper = p;
          break;
        }
        default:
          return upper;
      }
    }  // while(1)
  }
  Expression* _ParseBinaryImpl(const BinopParserParam* param,
                               Expression* (Parser::*UpperParserFunc)()) {
    BinaryExpr* p = AllocBinaryExpr();
    p->left = (this->*UpperParserFunc)();
    if (p->left == nullptr) return nullptr;
    while (true) {
      if (!param->test(TK.t)) break;
      p->opt = TK.t;
      CONSUME;
      p->right = (this->*UpperParserFunc)();
      BinaryExpr* new_p = AllocBinaryExpr();
      new_p->left = p;  //左结合
      p = new_p;
    }
    return p->left;
  }
#define _BPP(_x) TokenType::_x
#define _PARSE_BINOP_IMPL(_upper, ...)                     \
  static constexpr BinopParserParam param = {__VA_ARGS__}; \
  return _ParseBinaryImpl(&param, &Parser::_upper);
  Expression* ParseMulDivMod() {
    _PARSE_BINOP_IMPL(ParseInvokeOrGetMemberOrGetIndex, _BPP(MUL), _BPP(IDIV),
                      _BPP(FDIV), _BPP(MOD));
  }
  Expression* ParseAddSub() {
    _PARSE_BINOP_IMPL(ParseMulDivMod, _BPP(ADD), _BPP(SUB));
  }
  Expression* ParseBitop() {
    _PARSE_BINOP_IMPL(ParseAddSub, _BPP(BAND), _BPP(BOR), _BPP(BXOR), _BPP(SHL),
                      _BPP(SHR));
  }
  Expression* ParseCmpop() {
    _PARSE_BINOP_IMPL(ParseBitop, _BPP(EQ), _BPP(NEQ), _BPP(LT), _BPP(GT),
                      _BPP(LE), _BPP(GE));
  }
  Expression* ParseLogicop() {
    _PARSE_BINOP_IMPL(ParseCmpop, _BPP(AND), _BPP(OR));
  }
  Expression* ParseAssignop() {  // = += -= ... 右结合
    Expression* left = ParseLogicop();
    if (left == nullptr) return nullptr;
    switch (TK.t) {
      case TokenType::ASSIGN:
      case TokenType::ADD_ASSIGN:
      case TokenType::SUB_ASSIGN:
      case TokenType::MUL_ASSIGN:
      case TokenType::IDIV_ASSIGN:
      case TokenType::FDIV_ASSIGN:
      case TokenType::BAND_ASSIGN:
      case TokenType::BOR_ASSIGN:
      case TokenType::BXOR_ASSIGN:
        VERIFY(IsAssignableExpr(left));
        AssignExpr* p = AllocAssignExpr();
        p->opt = TK.t;
        p->left = (AssignableExpr*)left;
        CONSUME;
        p->right = ParseAssignop();  //右结合
        return p;
    }
    return left;
  }
  Expression* ParseBinary() { return ParseAssignop(); }
  Expression* ParseExpression() { return ParseAssignop(); }
  IfStat* ParseIF() {
    IfStat* p = AllocIfStat();
    ASSERT(TK.t == TokenType::IF);
    CONSUME;
    REQUIRE(TokenType::BK_SL);  //(
    p->cond = ParseExpression();
    VERIFY(p->cond);
    REQUIRE(TokenType::BK_SR);  //)
    p->then_stat = TryParseStatement();
    if (TK.t == TokenType::ELSE) {
      p->else_stat = TryParseStatement();
    }
    return p;
  }
  ReturnStat* ParseReturn() {
    ASSERT(TK.t == TokenType::RETURN);
    CONSUME;
    ReturnStat* p = AllocReturnStat();
    if (TK.t == TokenType::SEMI) {
      p->expr = nullptr;
    } else {
      p->expr = ParseExpression();
      VERIFY(p->expr);
    }
    return p;
  }
  LoopStat* ParseWhile() {  // TODO
    ASSERT(TK.t == TokenType::WHILE);
    CONSUME;
    LoopStat* p = AllocLoopStat();
    p->loop_type = LoopStat::Type::WHILE;
    REQUIRE(TokenType::BK_SL);
    p->cond = ParseExpression();
    VERIFY(p->cond);
    REQUIRE(TokenType::BK_SR);
    p->body = TryParseStatement();
    VERIFY(p->body);
    return p;
  }
  LoopStat* ParseFor() {  // TODO
    ASSERT(TK.t == TokenType::FOR);
    CONSUME;
    LoopStat* p = AllocLoopStat();
    p->loop_type = LoopStat::Type::FOR;
    REQUIRE(TokenType::BK_SL);
    if (TK.t != TokenType::SEMI) {
      p->init = TryParseStatement();
      VERIFY(p->init);
    }
    CONSUME;  //;
    if (TK.t != TokenType::SEMI) {
      p->cond = ParseExpression();
      VERIFY(p->cond);
    }
    CONSUME;  //;
    if (TK.t != TokenType::BK_SR) {
      p->after = TryParseStatement();
      VERIFY(p->after);
    }
    REQUIRE(TokenType::BK_SR);
    p->body = TryParseStatement();
    VERIFY(p->body);
    return p;
  }

  VarDecl* ParseVarDecl() {
    ASSERT(TK.t == TokenType::VAR);
    CONSUME;
    VarDecl* p = AllocVarDecl();
    if (TK.t != TokenType::SYMBOL) VERIFY(0);

    while (TK.t == TokenType::SYMBOL) {
      Expression* init = nullptr;
      Handle<String> name = Handle<String>::cast(TK.v);
      CONSUME;
      if (TK.t == TokenType::ASSIGN) {
        CONSUME;
        init = ParseExpression();
      }
      p->decl.push({name, init});
      if (TK.t == TokenType::COMMA) {
        CONSUME;
        continue;
      } else if (TK.t == TokenType::SEMI) {
        break;
      } else {
        VERIFY(0);
      }
    }
    return p;
  }
  BlockStat* ParseBlock() {
    BlockStat* p = AllocBlockStat();
    REQUIRE(TokenType::BK_LL);
    while (TK.t != TokenType::BK_LR) {
      Statement* s = TryParseStatement();
      if (s == nullptr) break;
      p->stat.push(s);
    }
    REQUIRE(TokenType::BK_LR);
    return p;
  }
  Statement* TryParseStatement() {
  l_begin:
    switch (TK.t) {
      case TokenType::IF:
        return ParseIF();
      case TokenType::FOR:
        return ParseFor();
      case TokenType::WHILE:
        return ParseWhile();
      case TokenType::BREAK:
        CONSUME;
        return AllocBreakStat();
      case TokenType::CONTINUE:
        CONSUME;
        return AllocContinueStat();
      case TokenType::RETURN:
        return ParseReturn();
      case TokenType::FUNC:
        return ParseFunctionDecl();
      case TokenType::VAR:
        return ParseVarDecl();
      case TokenType::BK_LL:  //{
        return ParseBlock();
      case TokenType::SEMI:
        CONSUME;
        goto l_begin;
        break;
      case TokenType::END:
        return nullptr;
    }
    ExpressionStat* p = AllocExpressionStat();
    p->expr = ParseExpression();
    if (p->expr == nullptr) return nullptr;
    return p;
  }

  FuncDecl* ParseFunctionDecl() {
    ASSERT(TK.t == TokenType::FUNC);
    CONSUME;
    if (TK.t != TokenType::SYMBOL) VERIFY(0);
    FuncDecl* p = AllocFuncDecl();
    p->name = Handle<String>::cast(TK.v);
    CONSUME;
    REQUIRE(TokenType::BK_SL);  // (
    if (TK.t == TokenType::SYMBOL) {
      while (true) {
        if (TK.t != TokenType::SYMBOL) VERIFY(0);
        p->param.push(Handle<String>::cast(TK.v));
        CONSUME;
        if (TK.t != TokenType::COMMA) break;
        CONSUME;
      };
    }
    REQUIRE(TokenType::BK_SR);  // )
    p->body = ParseBlock();
    return p;
  }

 public:
  Parser() : m_ts(nullptr) {}
  FuncDecl* ParseModule(Handle<String> s) {
    m_ts = new TokenStream(s->cstr());
    FuncDecl* p = AllocFuncDecl();
    p->name = Factory::NewString("#global");
    p->body = AllocBlockStat();
    while (TK.t != TokenType::END) {
      Statement* s = TryParseStatement();
      if (s == nullptr && TK.t == TokenType::END) break;
      VERIFY(s);
      p->body->stat.push(s);
    }
    delete m_ts;
    m_ts = nullptr;
    return p;
  }
};
struct VarCtx {
  Handle<String> name;
};
struct ExtVarCtx {
  Handle<String> name;
  uint16_t pos;
};
struct FunctionCtx {
  Handle<String> name;
  ZoneList<VarCtx> var;
  ZoneList<ExtVarCtx> extvar;
  ZoneList<Handle<Object>> kpool;
  ZoneList<FunctionCtx*> inner_func;
  ZoneList<uint8_t> cmd;
  size_t top;
  size_t max_stack;
  size_t param_cnt;
};
/*
loop:
init
cond
body
continue:
after
break:
*/
enum class Codepos {};
struct LoopCtx {
  ZoneList<Codepos> continue_pos;
  ZoneList<Codepos> break_pos;
};
inline FunctionCtx* AllocFunctionCtx() {
  FunctionCtx* p =
      (FunctionCtx*)CompilingMemoryZone::Alloc(sizeof(FunctionCtx));
  new (p) FunctionCtx();
  return p;
}
inline LoopCtx* AllocLoopCtx() {
  LoopCtx* p = (LoopCtx*)CompilingMemoryZone::Alloc(sizeof(FunctionCtx));
  new (p) LoopCtx();
  return p;
}
// TODO: stroe后的pop
class CodeGenerator : public ASTVisitor {
  FunctionCtx* ctx;
  LoopCtx* loop_ctx;

 public:
  CodeGenerator() : ctx(nullptr), loop_ctx(nullptr) {}
  Handle<SharedFunctionData> Generate(FuncDecl* fd) {
    Visit(fd);
    return Translate(ctx);
  }

 private:
  //将FunctionCtx转换为SharedFunctionData
  static inline Handle<SharedFunctionData> Translate(FunctionCtx* ctx) {
    Handle<SharedFunctionData> sfd = Factory::NewSharedFunctionData();
    sfd->name = *ctx->name;
    sfd->max_stack = ctx->max_stack;
    sfd->instructions = *Factory::NewInstructionArray(ctx->cmd.size());
    memcpy(sfd->instructions->begin(), ctx->cmd.begin(),
           sizeof(Cmd) * ctx->cmd.size());
    sfd->vars = *Factory::NewFixedArray(ctx->var.size());
    for (size_t i = 0; i < ctx->var.size(); i++) {
      Handle<VarData> vd = Factory::NewVarData();
      vd->name = *ctx->var[i].name;
      sfd->vars->set(i, *vd);
    }
    sfd->extvars = *Factory::NewFixedArray(0);
    sfd->kpool = *Factory::NewFixedArray(ctx->kpool.size());
    for (size_t i = 0; i < ctx->kpool.size(); i++) {
      sfd->kpool->set(i, *ctx->kpool[i]);
    }
    sfd->inner_func = *Factory::NewFixedArray(ctx->inner_func.size());
    for (size_t i = 0; i < ctx->inner_func.size(); i++) {
      sfd->inner_func->set(i, *Translate(ctx->inner_func[i]));
    }
    return sfd;
  }

 private:
  Codepos CurrentPos() { return (Codepos)ctx->cmd.size(); }
  Codepos PrepareJump() {
    Codepos cp = CurrentPos();
    Append(Opcode::JMP);
    Append((uint16_t)0);
    return cp;
  }
  Codepos PrepareJumpIf(bool cond) {
    Codepos cp = CurrentPos();
    Append(cond ? Opcode::JMP_T : Opcode::JMP_F);
    Append((uint16_t)0);
    return cp;
  }
  void ApplyJump(Codepos from, Codepos to) {
    Opcode op = (Opcode)ctx->cmd[(int)from];
    int16_t* jp = (int16_t*)&ctx->cmd[(int)from + 1];
    ASSERT(op == Opcode::JMP || op == Opcode::JMP_T || op == Opcode::JMP_F);
    ASSERT(*jp == 0);
    int diff = (int)to - (int)from;
    ASSERT(diff >= INT16_MIN && diff <= INT16_MAX);
    *jp = (int16_t)diff;
  }
  void Append(uint8_t v) { ctx->cmd.push(v); }
  void Append(uint16_t v) {
    ctx->cmd.push(*((uint8_t*)&v + 0));
    ctx->cmd.push(*((uint8_t*)&v + 1));
    ASSERT(v == *(uint16_t*)&ctx->cmd[ctx->cmd.size() - 2]);
  }
  void Append(uint32_t v) {
    ctx->cmd.push(*((uint8_t*)&v + 0));
    ctx->cmd.push(*((uint8_t*)&v + 1));
    ctx->cmd.push(*((uint8_t*)&v + 2));
    ctx->cmd.push(*((uint8_t*)&v + 3));
    ASSERT(v == *(uint32_t*)&ctx->cmd[ctx->cmd.size() - 4]);
  }
  void Append(int16_t v) { Append(*(uint16_t*)&v); }
  void Append(Opcode op) { Append((uint8_t)op); }
  uint16_t AddVar(Handle<String> name) {
    uint16_t pos = (uint16_t)ctx->var.size();
    VarCtx vc;
    vc.name = name;
    ctx->var.push(vc);
    return pos;
  }
  uint16_t FindVar(Handle<String> name) {
    for (size_t i = 0; i < ctx->var.size(); i++) {
      if (String::Equal(*name, *ctx->var[i].name)) {
        return (uint16_t)i;
      }
    }
    return -1;
  }
  uint16_t FindConst(Handle<Object> v) {
    for (size_t i = 0; i < ctx->kpool.size(); i++) {
      if (basic_obj_equal(*ctx->kpool[i], *v)) {
        return (uint16_t)i;
      }
    }
    uint16_t ret = (uint16_t)ctx->kpool.size();
    ctx->kpool.push(v);
    return ret;
  }
  void LoadK(Handle<Object> v) {
    ++ctx->top;
    ctx->max_stack = std::max(ctx->max_stack, ctx->top);
    uint16_t pos = FindConst(v);
    Append(Opcode::LOADK);
    Append(pos);
  }
  //尝试生成LoadL指令，若成功，返回true
  bool LoadL(Handle<String> name) {
    ++ctx->top;
    ctx->max_stack = std::max(ctx->max_stack, ctx->top);
    uint16_t pos = FindVar(name);
    if (pos == (uint16_t)-1) return false;
    Append(Opcode::LOADL);
    Append((uint16_t)pos);
    return true;
  }
  //尝试生成Closure指令，若成功，返回true
  bool Closure(Handle<String> name) {
    for (size_t i = 0; i < ctx->inner_func.size(); i++) {
      if (String::Equal(*ctx->inner_func[i]->name, *name)) {
        Append(Opcode::CLOSURE);
        Append((uint16_t)i);
        return true;
      }
    }
    return false;
  }

 private:
  // 通过 ASTVisitor 继承
  virtual void VisitExpressionStat(ExpressionStat* node);
  virtual void VisitLiteral(Literal* node);
  virtual void VisitBlockStat(BlockStat* node);
  virtual void VisitIfStat(IfStat* node);
  virtual void VisitLoopStat(LoopStat* node);
  virtual void VisitVarDecl(VarDecl* node);
  virtual void VisitFuncDecl(FuncDecl* node);
  virtual void VisitReturnStat(ReturnStat* node);
  virtual void VisitBreakStat(BreakStat* node);
  virtual void VisitContinueStat(ContinueStat* node);
  virtual void VisitVarExpr(VarExpr* node);
  virtual void VisitMemberExpr(MemberExpr* node);
  virtual void VisitIndexExpr(IndexExpr* node);
  virtual void VisitUnaryExpr(UnaryExpr* node);
  virtual void VisitBinaryExpr(BinaryExpr* node);
  virtual void VisitAssignExpr(AssignExpr* node);
  virtual void VisitCallExpr(CallExpr* node);
};

}  // namespace internal
}  // namespace rapid