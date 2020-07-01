//
// Created by 011295 on 2020/6/17.
//

#ifndef SHIOAJICPP_SRC_SHIOAJI_IMPL_H_
#define SHIOAJICPP_SRC_SHIOAJI_IMPL_H_
namespace shioaji{
  class Session {
   private:
    std::string token;
   public:
    Session();
    virtual ~Session();
  };
}

#endif //SHIOAJICPP_SRC_SHIOAJI_IMPL_H_
