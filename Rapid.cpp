#define _CRT_SECURE_NO_WARNINGS 1
#include <array>
#include <chrono>
#include <iostream>
#include <unordered_set>

#include "ast.h"
#include "ast_visualizer.h"
#include "compiler.h"
#include "engine.h"
#include "factory.h"
#include "handle.h"
#include "object.h"
using namespace rapid;
using namespace internal;
class A {
  int x;

 public:
  int y;
  auto acc_x() {}
};

void print_object(Object *obj) {
  if (obj->IsNull())
    printf("null");
  else if (obj->IsInteger())
    printf("%lld", Integer::cast(obj)->value());
  else if (obj->IsFloat())
    printf("%g", Float::cast(obj)->value());
  else if (obj->IsString())
    printf("\"%s\"", String::cast(obj)->cstr());
  else if (obj->IsSpecialValue())
    printf("%s", obj->IsNull() ? "null" : obj->IsTrue() ? "true" : "false");
  else
    printf("[object]");
}
void print_token(const Token &t) {
  printf("[%d,%d] ", t.row, t.col);
  switch (t.t) {
#define PRINT_ITERATOR(_t, _s) \
  case _t:                     \
    std::cout << (_s);         \
    break;
    TT_ITER_CONTROL(PRINT_ITERATOR);
    TT_ITER_KEWWORD(PRINT_ITERATOR);
    TT_ITER_OPERATOR(PRINT_ITERATOR);
    case TokenType::KVAL:
      printf("k ");
      print_object(t.v.ptr());
      break;
    case TokenType::SYMBOL:
      printf("sym %s", String::cast(t.v.ptr())->cstr());
      break;
  }
}
char buff[1024 * 1024 * 128];

void do_tokenlize(int64_t *ti, int64_t *cnt) {
  std::chrono::high_resolution_clock::time_point t1 =
      std::chrono::high_resolution_clock::now();
  HandleScope hs;
  TokenStream ts(buff);
  while (true) {
    if (ts.peek().t == TokenType::END) break;
    Token &t = ts.peek();
    // print_token(t);
    // putchar('\n');
    ++*cnt;
    ts.consume();
  }
  std::chrono::high_resolution_clock::time_point t2 =
      std::chrono::high_resolution_clock::now();
  std::chrono::nanoseconds tt = t2 - t1;
  *ti += tt.count();
}

void test_parse(Handle<String> code) {
  Parser parser;
  std::chrono::high_resolution_clock::time_point t =
      std::chrono::high_resolution_clock::now();
  FuncDecl *ast = parser.ParseModule(code);
  std::chrono::nanoseconds tt = std::chrono::high_resolution_clock::now() - t;
  printf("%.2fus.\n", tt.count() / 1000.0);
  printf("%.2fms.\n", tt.count() / 1000.0 / 1000.0);
  printf("%.2fKiB.\n", CompilingMemoryZone::GetUsage() / 1024.0);
  return;
  Handle<String> vs = VisualizeAST(ast);
  FILE *f = fopen("graph.md", "w");
  fprintf(f, "```mermaid\ngraph LR\n");
  fprintf(f, "%s", vs->cstr());
  fprintf(f, "```\n");
  fclose(f);
}
void test_compile(Handle<String> code) {
  Parser parser;
  FuncDecl *ast = parser.ParseModule(code);
  if (ast == nullptr) {
    printf(Executer::GetException()->info()->cstr());
    fflush(stdout);
    exit(0);
  }
  FILE *f = fopen("graph.md", "w");
  fprintf(f, "```mermaid\ngraph LR\n");
  fprintf(f, "%s", VisualizeAST(ast)->cstr());
  fprintf(f, "```\n");
  fclose(f);

  CodeGenerator cg;
  Handle<SharedFunctionData> sfd = cg.Generate(ast);
  if (sfd.empty()) {
    printf(Executer::GetException()->info()->cstr());
    fflush(stdout);
    exit(0);
  }

  f = fopen("btc.txt", "w");
  fprintf(f, "%s", VisualizeByteCode(sfd)->cstr());
  fclose(f);
  Parameters param(Heap::NullValue(), nullptr, 0);
  Handle<Object> ret = Executer::CallFunction(sfd, param);
}
int main() {
  freopen("log.txt", "w", stderr);
  FILE *f = fopen("test.ra", "r");
  size_t siz = fread(buff, 1, 1024 * 1024 * 128, f);
  fclose(f);
  buff[siz] = '\0';
  Engine::Init();
  HandleScope hs;
  Handle<String> code = Factory::NewString(buff);

  CompilingMemoryZone::PrepareAlloc();
  test_compile(code);
  // HandleScope hs;
  // int64_t cnt = 0;
  // int64_t t = 0;
  // String* s = Heap::AllocString("123", 3);
  // Handle<FixedArray> hfa = Factory::NewFixedArray(10);
  // Handle<Array> ha = Factory::NewArray();
  // ha->push(Heap::NullValue());
  // Handle<Object> ho;
  // Handle<String> h = Factory::NewString("123asd");
  // for (int i = 0; i < 10; i++) {
  //  do_tokenlize(&t, &cnt);
  //  // printf("oc pre: %lld\n", Heap::ObjectCount());
  //  Heap::DoGC();
  //  // printf("oc nxt: %lld\n", Heap::ObjectCount());
  //}
  // printf("time: %fms.\n", (double)t / 1000 / 1000);
  // printf("token count: %lld\n", cnt);
  //// printf("%f tokens per ns\n", (double)cnt / t);
  // printf("%.2f tokens per us\n", (double)cnt / t * 1000);
  // printf("%.2f tokens per ms\n", (double)cnt / t * 1000 * 1000);
  // printf("%.2f tokens per s\n", (double)cnt / t * 1000 * 1000 * 1000);
  //{
  //  HandleScope hs;
  //  Handle<Table> tb = Factory::NewTable();
  //  tb->set(Factory::NewString("a").ptr(), Integer::FromInt64(1));
  //  tb->set(Factory::NewString("b").ptr(), Integer::FromInt64(2));
  //  tb->set(Factory::NewString("c").ptr(), Integer::FromInt64(3));
  //  tb->set(Factory::NewString("d").ptr(), Factory::NewString("a").ptr());
  //  tb->set(Factory::NewString("e").ptr(), Factory::NewString("b").ptr());
  //  tb->set(Factory::NewString("f").ptr(), Factory::NewString("c").ptr());
  //  Heap::DoGC();
  //  Object* p;
  //  p = tb->get(Factory::NewString("a").ptr());
  //  int64_t x;
  //  x = Integer::cast(p)->value();
  //  p = tb->get(Factory::NewString("b").ptr());
  //  x = Integer::cast(p)->value();
  //  p = tb->get(Factory::NewString("c").ptr());
  //  x = Integer::cast(p)->value();
  //  p = tb->get(Factory::NewString("d").ptr());
  //  p = tb->get(Factory::NewString("e").ptr());
  //  p = tb->get(Factory::NewString("f").ptr());
  //}
  // arr->resize(0);
  // Handle<Object> s = Factory::NewString("123abc");
  Heap::DoGC();
  // std::cout << s->cstr() << std::endl;

  return 0;
}