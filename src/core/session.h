#ifndef SHIOAJICPP_SRC_CORE_SESSION_H_
#define SHIOAJICPP_SRC_CORE_SESSION_H_
#include <string>
namespace shioaji{
  class  Session {
   private:
    std::string token;
   public:
    Session();
    virtual ~Session();
  };
}
#endif //SHIOAJICPP_SRC_CORE_SESSION_H_
