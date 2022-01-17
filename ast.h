#pragma once
#include "bytecode.h"
#include "handle.h"
namespace rapid {
namespace internal {
struct ZonePage;
/*
提供对编译时简单小对象的内存快速分配
其内部申请多个大块内存页，并在其上连续进行对象的分配（如同在栈上分配内存）
不能释放单个对象内存，仅能通过FreeAll()一次性释放所有内存
注意FreeAll时不会调用对象的析构函数
*/
class CompilingMemoryZone {
 public:
  static CompilingMemoryZone *Create();
  static void PrepareAlloc();
  static void FreeAll();
  static size_t GetUsage();

  static void *Alloc(size_t size);
};
template <class T>
class ZoneList {
  static_assert(std::is_trivially_destructible_v<T>, "<T> 必须可平凡析构");

 private:
  T *m_p;
  size_t m_siz, m_cap;

 private:
  void change_capacity(size_t new_cap) {
    T *oldp = m_p;
    m_p = (T *)CompilingMemoryZone::Alloc(sizeof(T) * new_cap);
    VERIFY(m_p != nullptr);
    for (size_t i = 0; i < m_siz; i++) new (m_p + i) T(std::move(oldp[i]));
    // Free oldp
    m_cap = new_cap;
  }

 public:
  ZoneList() : m_p(nullptr), m_siz(0), m_cap(0) {}
  ~ZoneList() = default;
  T *begin() const { return m_p; }
  T *end() const { return m_p + m_siz; }
  size_t size() const { return m_siz; }
  size_t capacity() const { return m_cap; }
  T &operator[](size_t pos) { return m_p[pos]; }
  const T &operator[](size_t pos) const { return m_p[pos]; }
  void push(const T &v) {
    reserve(m_siz + 1);
    new (m_p + m_siz) T(v);
    ++m_siz;
  }
  void pop() {
    ASSERT(m_siz > 0);
    --m_siz;
  }
  void resize(size_t new_size, const T val = {}) {
    if (new_size < m_siz) {
      m_siz = new_size;
      return;
    }
    reserve(new_size);
    for (size_t i = m_siz; i < new_size; i++) {
      m_p[i] = val;
    }
    m_siz = new_size;
  }
  void reserve(size_t size) {
    if (size <= m_cap) return;
    change_capacity(std::max(size, m_cap << 1));
  }
  void shrink_to_fit() { change_capacity(m_siz); }
  void clear() { m_siz = 0; }
  const T &front() { return *m_p; }
  const T &back() { return m_p[m_siz - 1]; }
};
#define ITER_ASTNODE(V) \
  V(ExpressionStat)     \
  V(Literal)            \
  V(BlockStat)          \
  V(IfStat)             \
  V(LoopStat)           \
  V(VarDecl)            \
  V(FuncDecl)           \
  V(ReturnStat)         \
  V(BreakStat)          \
  V(ContinueStat)       \
  V(VarExpr)            \
  V(MemberExpr)         \
  V(IndexExpr)          \
  V(UnaryExpr)          \
  V(BinaryExpr)         \
  V(AssignExpr) V(CallExpr) V(ThisExpr) V(ParamsExpr) V(ImportExpr)

enum class AstNodeType {
#define AstNodeType_ITER(t) t,
  ITER_ASTNODE(AstNodeType_ITER)
};
struct AstNode {
  AstNodeType type;
  int row, col;
};
struct Statement : public AstNode {};
struct Expression : public AstNode {};
struct AssignableExpr : public Expression {};

struct ExpressionStat : public Statement {
  Expression *expr;
};
struct BlockStat : public Statement {
  ZoneList<Statement *> stat;
};
struct Literal : public Expression {
  Handle<Object> value;
};
struct ThisExpr : public AssignableExpr {};
struct ParamsExpr : public AssignableExpr {};
struct ImportExpr : public AssignableExpr {};
struct IfStat : public Statement {
  Expression *cond;
  Statement *then_stat, *else_stat;
};
struct LoopStat : public Statement {
  enum class Type { FOR, WHILE, DO };
  Type loop_type;
  Statement *init;
  ExpressionStat *after;
  Expression *cond;
  Statement *body;
};
struct SingleVarDecl {
  Handle<String> name;
  Expression *init;
};
struct VarDecl : public Statement {
  ZoneList<SingleVarDecl> decl;
};
struct FuncDecl : public Statement {
  Handle<String> name;
  ZoneList<Handle<String>> param;
  BlockStat *body;
};
// struct TryCatchStat : public Statement {
//  BlockStat* try_body;
//  BlockStat* catch_body;
//};
struct ReturnStat : public Statement {
  Expression *expr;
};
struct BreakStat : public Statement {};
struct ContinueStat : public Statement {};
struct VarExpr : public AssignableExpr {
  Handle<String> name;
};
struct MemberExpr : public AssignableExpr {
  Expression *target;
  Handle<String> name;
};
struct IndexExpr : public AssignableExpr {
  Expression *target;
  Expression *index;
};
struct BinaryExpr : public Expression {
  Expression *left;
  Expression *right;
  TokenType opt;
};
struct UnaryExpr : public Expression {
  Expression *expr;
  TokenType opt;
};
struct CallExpr : public Expression {
  Expression *callee;
  ZoneList<Expression *> params;
};
struct AssignExpr : public Expression {
  AssignableExpr *left;
  Expression *right;
  TokenType opt;
};

inline bool IsAssignableExpr(AstNode *node) {
  switch (node->type) {
    case AstNodeType::VarExpr:
    case AstNodeType::MemberExpr:
    case AstNodeType::IndexExpr:
      return true;
  }
  return false;
}

template <class T>
inline T *AllocNode(int, int);
#define AllocNode_ITERATOR(T)                          \
  template <>                                          \
  inline T *AllocNode<T>(int row, int col) {           \
    T *p = (T *)CompilingMemoryZone::Alloc(sizeof(T)); \
    new (p) T();                                       \
    p->type = AstNodeType::T;                          \
    p->row = row;                                      \
    p->col = col;                                      \
    return p;                                          \
  }                                                    \
  inline T *Alloc##T(int row, int col) { return AllocNode<T>(row, col); }
ITER_ASTNODE(AllocNode_ITERATOR)
#undef AllocNode_ITERATOR
class ASTVisitor {
 public:
  virtual ~ASTVisitor(){};
  void Visit(AstNode *node) {
#define ASTVisitor_DISPATCH_VIS_ITER(T)                 \
  case AstNodeType::T:                                  \
    return this->Visit##T(reinterpret_cast<T *>(node)); \
    break;
    switch (node->type) {
      ITER_ASTNODE(ASTVisitor_DISPATCH_VIS_ITER);
      default:
        ASSERT(0);
        break;
    }
#undef ASTVisitor_DISPATCH_VIS_ITER
  }
#define ASTVisitor_DEF_VIS_ITER(T)    \
  virtual void Visit##T(T *node) = 0; \
  void Visit(T *node) { return Visit##T(node); }
  ITER_ASTNODE(ASTVisitor_DEF_VIS_ITER)
#undef ASTVisitor_DEF_VIS_ITER
};
// class ASTVisitor;
// class AstNode {
// public:
//  virtual ~AstNode() {}
//  virtual void Accept(ASTVisitor* v) = 0;
//};
//
// class Statement : public AstNode {
// public:
//};
// class Expression : public AstNode {
// public:
//};
// class ExpressionStat : public Statement {
//  Expression* m_exp;
//
// public:
//  ExpressionStat(Expression* exp) : m_exp(exp) {}
//  Expression* expr() { return m_exp; }
//};
// class Literal : public Expression {
//  Handle<Object> m_v;
//
// public:
//  Literal(Handle<Object> v) : m_v(v) {}
//  Handle<Object> value() { return m_v; }
//};
// class IfStat : public Statement {
//  Expression* m_cond;
//  Statement *m_then, *m_else;
//
// public:
//  Expression* condition() { return m_cond; }
//  Statement* then_stat() { return m_then; }
//  Statement* else_stat() { return m_else; }
//};
// class VarDecl : public Statement {};
// class FuncDecl : public Statement {};
// class LoopStat : public Statement {
//  enum class Type { FOR, WHILE, DO };
//  Type m_type;
//  Statement *m_init, *m_after;
//  Expression* m_cond;
//
// public:
//  LoopStat(Type type, Statement* init, Expression* cond, Statement* after)
//      : m_type(type), m_init(init), m_cond(cond), m_after(after) {}
//  Statement* init() { return m_init; }
//  Expression* cond() { return m_cond; }
//  Statement* after() { return m_after; }
//  Type type() { return m_type; }
//};
// class TryCatchStat : public Statement {};
// class ReturnStat : public Statement {
//  Expression* m_exp;
//
// public:
//  ReturnStat(Expression* expr) : m_exp(expr) {}
//  Expression* expr() { return m_exp; }
//};
// class BreakStat : public Statement {};
// class ContinueStat : public Statement {};
//
// class AssignableExpr : public Expression {};
//
// class VarExpr : public AssignableExpr {};
// class MemberExpr : public AssignableExpr {};
// class IndexExpr : public AssignableExpr {};
//
// class BinopExpr : public Expression {};
// class UnaryExpr : public Expression {};
// class AssignExpr : public Expression {
//  AssignableExpr* m_left;
//  Expression* m_right;
//  TokenType m_op;
//
// public:
//  AssignExpr(TokenType op, AssignableExpr* left, Expression* right)
//      : m_op(op), m_left(left), m_right(right) {
//    //ASSERT();
//  }
//};
// class ASTVisitor {
// public:
//  void Visit(AstNode* node) { node->Accept(this); }
//
//};

}  // namespace internal
}  // namespace rapid