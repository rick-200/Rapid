#pragma once
#include "handle.h"
#include "preprocessors.h"
namespace rapid {
namespace internal {
/*




*/

enum class Opcode : uint8_t {
  NOP,
  LOADL,
  STOREL,
  LOADK,
  LOADE,
  STOREE,
  COPY,  //复制栈顶并push到栈上
  POP,

  ADD,
  SUB,
  MUL,
  IDIV,
  FDIV,
  MOD,

  BAND,
  BOR,
  BNOT,
  BXOR,
  SHL,
  SHR,

  NOT,
  AND,
  OR,

  LT,   //<
  GT,   //>
  LE,   //<=
  GE,   //>=
  EQ,   //==
  NEQ,  //!=

  GET_M,
  GET_I,
  SET_M,
  SET_I,

  JMP,
  JMP_T,
  JMP_F,

  NEG,
  ACT,

  CALL,
  RET,
  RETNULL,
  CLOSURE,
  CLOSURE_SELF,

  PLACE_HOLDER,

  __SIZE,

};

enum class TokenType {
  NUL,
  END,
  SYMBOL,
  KVAL,

  FUNC,
  RETURN,
  VAR,
  IF,
  ELSE,
  FOR,
  WHILE,
  CONTINUE,
  BREAK,
  EXPORT,
  CONST,
  TRY,
  CATCH,

  BK_SL,
  BK_SR,
  BK_ML,
  BK_MR,
  BK_LL,
  BK_LR,
  COMMA,
  DOT,
  SEMI,

  ADD,  //既代表二元加法，又代表一元‘正’
  SUB,  //既代表二元减法，又代表一元‘负’
  MUL,
  IDIV,
  FDIV,
  MOD,
  BAND,
  BOR,
  BNOT,
  BXOR,
  SHL,
  SHR,

  NOT,
  AND,
  OR,

  ASSIGN,
  ADD_ASSIGN,
  SUB_ASSIGN,
  MUL_ASSIGN,
  IDIV_ASSIGN,
  FDIV_ASSIGN,
  MOD_ASSIGN,
  BAND_ASSIGN,
  BOR_ASSIGN,
  BNOT_ASSIGN,
  BXOR_ASSIGN,
  SHL_ASSIGN,
  SHR_ASSIGN,

  LT,
  GT,
  LE,
  GE,
  NEQ,
  EQ,
  __SIZE,
};
#define TT_ITER_KEWWORD(V)           \
  V(TokenType::FUNC, "func")         \
  V(TokenType::RETURN, "return")     \
  V(TokenType::VAR, "var")           \
  V(TokenType::IF, "if")             \
  V(TokenType::ELSE, "else")         \
  V(TokenType::FOR, "for")           \
  V(TokenType::WHILE, "while")       \
  V(TokenType::CONTINUE, "continue") \
  V(TokenType::BREAK, "break")       \
  V(TokenType::EXPORT, "export")     \
  V(TokenType::CONST, "const")       \
  V(TokenType::TRY, "try")           \
  V(TokenType::CATCH, "catch")

#define TT_ITER_CONTROL(V) \
  V(TokenType::BK_SL, "(") \
  V(TokenType::BK_SR, ")") \
  V(TokenType::BK_ML, "[") \
  V(TokenType::BK_MR, "]") \
  V(TokenType::BK_LL, "{") \
  V(TokenType::BK_LR, "}") \
  V(TokenType::COMMA, ",") \
  V(TokenType::DOT, ".")   \
  V(TokenType::SEMI, ";")

#define TT_ITER_OPERATOR(V)        \
  V(TokenType::ADD, "+")           \
  V(TokenType::SUB, "-")           \
  V(TokenType::MUL, "*")           \
  V(TokenType::IDIV, "//")         \
  V(TokenType::FDIV, "/")          \
  V(TokenType::MOD, "%")           \
  V(TokenType::BNOT, "~")          \
  V(TokenType::BAND, "&")          \
  V(TokenType::BOR, "|")           \
  V(TokenType::SHL, "<<")          \
  V(TokenType::SHR, ">>")          \
  V(TokenType::NOT, "!")           \
  V(TokenType::AND, "&&")          \
  V(TokenType::OR, "||")           \
  V(TokenType::ASSIGN, "=")        \
  V(TokenType::ADD_ASSIGN, "+=")   \
  V(TokenType::SUB_ASSIGN, "-=")   \
  V(TokenType::MUL_ASSIGN, "*=")   \
  V(TokenType::IDIV_ASSIGN, "//=") \
  V(TokenType::FDIV_ASSIGN, "/=")  \
  V(TokenType::MOD_ASSIGN, "%=")   \
  V(TokenType::BAND_ASSIGN, "&=")  \
  V(TokenType::BOR_ASSIGN, "|=")   \
  V(TokenType::BNOT_ASSIGN, "~=")  \
  V(TokenType::SHL_ASSIGN, "<<=")  \
  V(TokenType::SHR_ASSIGN, ">>=")  \
  V(TokenType::LT, "<")            \
  V(TokenType::GT, ">")            \
  V(TokenType::LE, "<=")           \
  V(TokenType::GE, ">=")           \
  V(TokenType::NEQ, "!=")          \
  V(TokenType::EQ, "==")

constexpr bool IsOperator(TokenType tt) {
  switch (tt) {
#define IsOperator_ITER(t, s) case t:
    TT_ITER_OPERATOR(IsOperator_ITER)
    return true;
  }
  return false;
}
constexpr bool IsUnaryop(TokenType tt) {
  switch (tt) {
    case TokenType::NOT:
    case TokenType::BNOT:
    case TokenType::ADD:
    case TokenType::SUB:
      return true;
  }
}
constexpr bool IsBinop(TokenType tt) {
  if (tt == TokenType::ADD || tt == TokenType::SUB)
    return true;  // ADD 和 SUB 既是一元又是二元
  return IsOperator(tt) && !IsUnaryop(tt);
}
// constexpr bool IsAssignop(TokenType tt) {}
typedef uint16_t DChar;
constexpr DChar make_dchar(char c1, char c2 = '\0') {
  return (((DChar)c2) << 8) | c1;
}
constexpr DChar make_dchar(const char (&s)[3]) {
  return make_dchar(s[0], s[1]);
}
constexpr DChar make_dchar(const char (&s)[2]) { return make_dchar(s[0]); }
constexpr char dchar_get1(DChar c) { return c & 0xFF; }
constexpr char dchar_get2(DChar c) { return c >> 8; }

struct Token {
  TokenType t;
  int row, col;
  Handle<Object> v;
};

}  // namespace internal
}  // namespace rapid