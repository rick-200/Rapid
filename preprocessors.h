#pragma once
#include <cassert>
#include <cstdint>
#include <new>
static_assert(sizeof(void*) == 8, "当前只能在64位机器上使用！");

#define ASSUME(exp) __assume(exp)

#ifdef _DEBUG
#define IF_DEBUG(...) __VA_ARGS__
#define ASSERT(exp) assert(exp)
#else
#define ASSERT(exp) ASSUME(exp)
#define IF_DEBUG(...) 
#endif
// VERIFY为开发中暂时使用，后期全部换成错误处理
#define VERIFY(exp) (assert(exp))

#define DISABLE_COPY(_t)  \
  _t(const _t&) = delete; \
  _t& operator=(const _t&) = delete;

#define DISABLE_NEW                                               \
  static void* operator new(size_t size) = delete;                \
  static void operator delete(void* pdead, size_t size) = delete; \
  static void* operator new[](size_t size) = delete;              \
  static void operator delete[](void* pdead, size_t size) = delete;

#define PTR_OFFSET(_p, _off) ((void*)((uint8_t*)(_p) + (intptr_t)(_off)))
#define __EVAL1(x) x
#define __EVAL4(x) __EVAL1(__EVAL1(__EVAL1(__EVAL1(x))))
#define __EVAL256(x) __EVAL4(__EVAL4(__EVAL4(__EVAL4(x))))
#define _EVAL(x) __EVAL4(x)
#define _EVAL_EX(x) __EVAL256(x)