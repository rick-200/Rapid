#define _CRT_SECURE_NO_WARNINGS 1
#include <array>
#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_set>

#include "ast.h"
#include "ast_visualizer.h"
#include "compiler.h"
#include "standerd_module.h"
#include "engine.h"
#include "factory.h"
#include "handle.h"
#include "native_object_interface.h"
#include "object.h"
using namespace rapid;
using namespace internal;

#define TEST_DIRECTORY "E:/Script/Rapid/test/"

char buff[1024 * 1024 * 128];

void do_tokenlize(int64_t* ti, int64_t* cnt) {
  std::chrono::high_resolution_clock::time_point t1 =
      std::chrono::high_resolution_clock::now();
  HandleScope hs;
  TokenStream ts(buff);
  while (true) {
    if (ts.peek().t == TokenType::END) break;
    Token& t = ts.peek();
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
  Parser parser(Factory::NewString("test.rs"));
  std::chrono::high_resolution_clock::time_point t =
      std::chrono::high_resolution_clock::now();
  FuncDecl* ast = parser.ParseModule(code);
  std::chrono::nanoseconds tt = std::chrono::high_resolution_clock::now() - t;
  printf("%.2fus.\n", tt.count() / 1000.0);
  printf("%.2fms.\n", tt.count() / 1000.0 / 1000.0);
  printf("%.2fKiB.\n", CompilingMemoryZone::GetUsage() / 1024.0);
  return;
  Handle<String> vs = VisualizeAST(ast);
  FILE* f = fopen(TEST_DIRECTORY "graph.md", "w");
  fprintf(f, "```mermaid\ngraph LR\n");
  fprintf(f, "%s", vs->cstr());
  fprintf(f, "```\n");
  fclose(f);
}
void test_compile(Handle<String> code) {
  Parser parser(Factory::NewString("test.rs"));
  FuncDecl* ast = parser.ParseModule(code);
  if (ast == nullptr) {
    printf(Executer::GetException()->info->cstr());
    fflush(stdout);
    exit(0);
  }
  FILE* f = fopen(TEST_DIRECTORY "graph.md", "w");
  fprintf(f, "```mermaid\ngraph LR\n");
  fprintf(f, "%s", VisualizeAST(ast)->cstr());
  fprintf(f, "```\n");
  fclose(f);

  CodeGenerator cg(Factory::NewString("test.rs"));
  Handle<FunctionData> fd = cg.Generate(ast);
  if (fd.empty()) {
    printf(Executer::GetException()->info->cstr());
    fflush(stdout);
    exit(0);
  }

  f = fopen(TEST_DIRECTORY "btc.txt", "w");
  fprintf(
      f, "%s",
      VisualizeByteCode(Handle<SharedFunctionData>(fd->shared_data))->cstr());
  fclose(f);
  Executer::RegisterModule(Factory::NewString("console"),
                           stdmodule::GetConsoleModule());
  Parameters param(nullptr, 0);

  std::chrono::high_resolution_clock::time_point t =
      std::chrono::high_resolution_clock::now();
  Handle<Object> ret = Executer::CallFunction(fd, param);
  std::chrono::nanoseconds tt = std::chrono::high_resolution_clock::now() - t;
  printf("%fms.\n", tt.count() / 1000000.0);
}

// void test_alloc() {
//  int x = 0;
//
//  std::chrono::high_resolution_clock::time_point t =
//      std::chrono::high_resolution_clock::now();
//  for (int i = 0; i < 100000000; i++) x ^= i;
//  std::chrono::nanoseconds tt1 = std::chrono::high_resolution_clock::now() -
//  t; t = std::chrono::high_resolution_clock::now(); for (int i = 0; i <
//  100000000; i++) {
//    void* p = malloc(sizeof(FunctionData));
//    free(p);
//    x ^= i;
//  }
//  std::chrono::nanoseconds tt2 = std::chrono::high_resolution_clock::now() -
//  t; printf("%d\n", x); printf("%llums.\n%llums.\n", tt1.count() / 1000 /
//  1000,
//         tt2.count() / 1000 / 1000);
//}

int main() {
  /*for (int i = 0; i < 10; i++) {
    UUID id = UUID::Create("Rick Wang");
    printf("{%llX, %llX}\n", id.get_val0(), id.get_val1());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return 0;*/
  // test_alloc();
  // return 0;
  freopen(TEST_DIRECTORY "log.txt", "w", stderr);
  FILE* f = fopen(TEST_DIRECTORY "test.ra", "r");
  size_t siz = fread(buff, 1, 1024 * 1024 * 128, f);
  fclose(f);
  buff[siz] = '\0';
  Engine::Init();

  {
    HandleScope hs;
    Handle<String> code = Factory::NewString(buff);

    CompilingMemoryZone::PrepareAlloc();
    test_compile(code);
  }

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
  //  Handle<Dictionary> tb = Factory::NewDictionary();
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
  Heap::PrintHeap();
  printf("%llu\n", Heap::ObjectCount());
  // std::cout << s->cstr() << std::endl;

  return 0;
}
/*


{17EF779BAD2E740, 15902275E1A5618E}
{17EF779BAE1DAE1, 15902275E1A53BF1}
{17EF779BAF1945C, 15902275E1A5485E}
{17EF779BB013CC5, 15902275E1A5CC17}
{17EF779BB10D544, 15902275E1A5659B}
{17EF779BB2012F5, 15902275E1A52899}

*/