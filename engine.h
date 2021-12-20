#pragma once
namespace rapid {
/*
对外接口
真正的实现在global
*/
class Engine {
 public:
  static Engine* Init(Engine* engine = nullptr);
  static Engine* Get();
  static void Destory();

 public:
};

}  // namespace rapid