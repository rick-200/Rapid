#pragma once
namespace rapid {
/*
����ӿ�
������ʵ����global
*/
class Engine {
 public:
  static Engine* Init(Engine* engine = nullptr);
  static Engine* Get();
  static void Destory();

 public:
};

}  // namespace rapid