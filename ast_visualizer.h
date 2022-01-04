#pragma once
#include "ast.h"
#include "stringbuilder.h"
namespace rapid {
namespace internal {

class VisualizerVisitor : public ASTVisitor {
  StringBuilder &m_sb;
  int m_cnt;
  int ret;

 public:
  VisualizerVisitor(StringBuilder &sb) : m_sb(sb), m_cnt(0), ret(0) {}
  // Í¨¹ý ASTVisitor ¼Ì³Ð
  void DefNode(Handle<Object> v) {
    ret = ++m_cnt;
    m_sb.AppendFormat("%X[\"", m_cnt);
    m_sb.AppendObject(v);
    m_sb.AppendString("\"]\n");
  }
  void DefNode(const char *s) {
    ret = ++m_cnt;
    m_sb.AppendFormat("%X[\"", m_cnt);
    m_sb.AppendString(s);
    m_sb.AppendString("\"]\n");
  }
  void Connect(int l, int r) { m_sb.AppendFormat("%X-->%X\n", l, r); }
  virtual void VisitExpressionStat(ExpressionStat *node) { Visit(node->expr); }
  virtual void VisitLiteral(Literal *node) {
    if (node->value->IsString()) {
      ret = ++m_cnt;
      m_sb.AppendFormat("%X[\"'", m_cnt);
      m_sb.AppendObject(node->value);
      m_sb.AppendString("'\"]\n");
    } else {
      DefNode(node->value);
    }
  }
  virtual void VisitBlockStat(BlockStat *node) {
    DefNode("block");
    int block = ret;
    for (size_t i = 0; i < node->stat.size(); i++) {
      Visit(node->stat[i]);
      Connect(block, ret);
    }
    ret = block;
  }
  virtual void VisitIfStat(IfStat *node) {
    DefNode("if");
    int if_ = ret;

    DefNode("cond");
    int cond = ret;
    Connect(if_, cond);
    Visit(node->cond);
    Connect(cond, ret);

    DefNode("then");
    int then = ret;
    Connect(if_, then);
    Visit(node->then_stat);
    Connect(then, ret);

    if (node->else_stat) {
      DefNode("else");
      int else_ = ret;
      Connect(if_, else_);
      Visit(node->else_stat);
      Connect(else_, ret);
    }
    ret = if_;
  }
  virtual void VisitLoopStat(LoopStat *node) {  // TODO
    if (node->loop_type == LoopStat::Type::FOR) {
      DefNode("for");
    } else if (node->loop_type == LoopStat::Type::WHILE) {
      DefNode("while");
    } else {
      ASSERT(0);
    }
    int loop = ret;
    if (node->init) {
      DefNode("init");
      int init = ret;
      Connect(loop, init);
      Visit(node->init);
      Connect(init, ret);
    }
    if (node->cond) {
      DefNode("cond");
      int cond = ret;
      Connect(loop, cond);
      Visit(node->cond);
      Connect(cond, ret);
    }
    if (node->after) {
      DefNode("after");
      int after = ret;
      Connect(loop, after);
      Visit(node->after);
      Connect(after, ret);
    }
    Visit(node->body);
    Connect(loop, ret);
    ret = loop;
  }
  virtual void VisitVarDecl(VarDecl *node) {
    DefNode("var");
    int var = ret;
    for (size_t i = 0; i < node->decl.size(); i++) {
      DefNode(node->decl[i].name);
      int name = ret;
      Connect(var, name);
      if (node->decl[i].init) {
        Visit(node->decl[i].init);
        int exp = ret;
        Connect(name, exp);
      }
    }
    ret = var;
  }
  virtual void VisitFuncDecl(FuncDecl *node) {
    int func = ++m_cnt;
    m_sb.AppendFormat("%X[\"func ", m_cnt);
    m_sb.AppendString(node->name);
    m_sb.AppendString("(");
    for (size_t i = 0; i < node->param.size(); i++) {
      m_sb.AppendString(node->param[i]);
    }
    m_sb.AppendString(")\"]\n");
    Visit(node->body);
    int body = ret;
    Connect(func, body);
    ret = func;
  }
  virtual void VisitReturnStat(ReturnStat *node) {
    DefNode("return");
    int return_ = ret;
    Visit(node->expr);
    int expr = ret;
    Connect(return_, expr);
    ret = return_;
  }
  virtual void VisitBreakStat(BreakStat *node) { DefNode("break"); }
  virtual void VisitContinueStat(ContinueStat *node) { DefNode("continue"); }
  virtual void VisitVarExpr(VarExpr *node) { DefNode(node->name); }
  virtual void VisitMemberExpr(MemberExpr *node) {
    DefNode(".");
    int dot = ret;
    Visit(node->target);
    int target = ret;
    DefNode(node->name);
    int name = ret;
    Connect(dot, target);
    Connect(dot, name);
    ret = dot;
  }
  virtual void VisitIndexExpr(IndexExpr *node) {
    DefNode("[]");
    int idxop = ret;
    Visit(node->target);
    int target = ret;
    Visit(node->index);
    int index = ret;
    Connect(idxop, target);
    Connect(idxop, index);
    ret = idxop;
  }
#define VisitBinaryExpr_ITER(t, s) \
  case t:                          \
    DefNode(s);                    \
    break;
#define SWITCH_DEF_NODE                     \
  switch (node->opt) {                      \
    TT_ITER_OPERATOR(VisitBinaryExpr_ITER); \
    default:                                \
      ASSERT(0);                            \
  }
  virtual void VisitUnaryExpr(UnaryExpr *node) {
    SWITCH_DEF_NODE;
    int op = ret;
    Visit(node->expr);
    int expr = ret;
    Connect(op, expr);
    ret = op;
  }
  virtual void VisitBinaryExpr(BinaryExpr *node) {
    SWITCH_DEF_NODE;
    int op = ret;
    Visit(node->left);
    int left = ret;
    Visit(node->right);
    int right = ret;
    Connect(op, left);
    Connect(op, right);
    ret = op;
  }
  virtual void VisitAssignExpr(AssignExpr *node) {
    SWITCH_DEF_NODE;
    int op = ret;
    Visit(node->left);
    int left = ret;
    Visit(node->right);
    int right = ret;
    Connect(op, left);
    Connect(op, right);
    ret = op;
  }
  virtual void VisitCallExpr(CallExpr *node) {
    DefNode("call");
    int call = ret;
    Visit(node->callee);
    int callee = ret;
    Connect(call, callee);
    DefNode("param:");
    int param = ret;
    Connect(call, param);
    for (size_t i = 0; i < node->params.size(); i++) {
      Visit(node->params[i]);
      Connect(param, ret);
    }
    ret = call;
  }
  virtual void VisitThisExpr(rapid::internal::ThisExpr *node) {
    DefNode("this");
  }
  virtual void VisitParamsExpr(rapid::internal::ParamsExpr *node) {
    DefNode("params");
  }
};

inline Handle<String> VisualizeAST(AstNode *node) {
  StringBuilder sb;
  VisualizerVisitor vv(sb);
  vv.Visit(node);
  return sb.ToString();
}

#define CASE_0(_op)        \
  case Opcode::_op:        \
    sb.AppendString(#_op); \
    ++pc;                  \
    break;
#define CASE_u16(_op)                                           \
  case Opcode::_op:                                             \
    sb.AppendString(#_op " ").AppendInt(*(uint16_t *)(pc + 1)); \
    pc += 3;                                                    \
    break;
#define CASE_s16(_op)                                          \
  case Opcode::_op:                                            \
    sb.AppendString(#_op " ").AppendInt(*(int16_t *)(pc + 1)); \
    pc += 3;                                                   \
    break;
inline Handle<String> VisualizeByteCode(Handle<SharedFunctionData> sfd) {
  StringBuilder sb;
  sb.AppendFormat("fuction '%s'@%p:\n", sfd->name->cstr(), *sfd);
  sb.AppendFormat("  instructions(%llu bytes):\n", sfd->instructions->length());
  uint8_t *pc = sfd->instructions->begin();
  while (pc - sfd->instructions->begin() < sfd->instructions->length()) {
    sb.AppendFormat("    %03d: ", pc - sfd->instructions->begin());
    switch ((Opcode)*pc) {
      CASE_0(NOP);
      CASE_u16(LOADL);
      CASE_u16(STOREL);
      CASE_u16(LOADK);
      CASE_u16(LOADE);
      CASE_u16(STOREE);
      CASE_0(LOAD_THIS);
      CASE_0(LOAD_PARAMS);
      CASE_0(COPY);
      CASE_0(ADD);
      CASE_0(SUB);
      CASE_0(MUL);
      CASE_0(IDIV);
      CASE_0(FDIV);
      CASE_0(MOD);
      CASE_0(BAND);
      CASE_0(BOR);
      CASE_0(BNOT);
      CASE_0(BXOR);
      CASE_0(SHL);
      CASE_0(SHR);
      CASE_0(POP);
      CASE_0(NOT);
      CASE_0(AND);
      CASE_0(OR);
      CASE_0(LT);
      CASE_0(GT);
      CASE_0(LE);
      CASE_0(GE);
      CASE_0(EQ);
      CASE_0(NEQ);
      CASE_0(GET_M);
      CASE_0(SET_M);
      CASE_0(GET_I);
      CASE_0(SET_I);
      CASE_0(NEG);
      CASE_0(ACT);
      CASE_u16(CALL);
      CASE_0(RET);
      CASE_0(RETNULL);
      CASE_s16(JMP);
      CASE_s16(JMP_T);
      CASE_s16(JMP_F);
      CASE_u16(CLOSURE);
      CASE_0(CLOSURE_SELF);

      default:
        ASSERT(0);
    }
    sb.AppendString("\n");
  }
  sb.AppendFormat("  locals(%llu):\n", sfd->vars->length());
  for (size_t i = 0; i < sfd->vars->length(); i++) {
    sb.AppendFormat("    %llu %s\n", i,
                    VarData::cast(sfd->vars->get(i))->name->cstr());
  }
  sb.AppendFormat("  consts(%llu):\n", sfd->kpool->length());
  for (size_t i = 0; i < sfd->kpool->length(); i++) {
    sb.AppendFormat("    %llu ", i);
    sb.AppendObject(Handle<Object>(sfd->kpool->get(i))).AppendString("\n");
  }
  sb.AppendFormat("  inner func(%llu):\n", sfd->inner_func->length());
  for (size_t i = 0; i < sfd->inner_func->length(); i++) {
    sb.AppendFormat(
        "    %llu '%s'@%p\n", i,
        SharedFunctionData::cast(sfd->inner_func->get(i))->name->cstr(),
        SharedFunctionData::cast(sfd->inner_func->get(i)));
  }
  sb.AppendString("\n");
  for (size_t i = 0; i < sfd->inner_func->length(); i++) {
    Handle<SharedFunctionData> h(
        SharedFunctionData::cast(sfd->inner_func->get(i)));
    sb.AppendString(VisualizeByteCode(h));
  }
  return sb.ToString();
}
}  // namespace internal
}  // namespace rapid