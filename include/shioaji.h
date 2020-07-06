#include <string>
#ifndef SHIOAJICPP_INCLUDE_SHIOAJI_H_
#define SHIOAJICPP_INCLUDE_SHIOAJI_H_

#endif //SHIOAJICPP_INCLUDE_SHIOAJI_H_

namespace shioaji {
  class Session;
  class Shioaji{
    private:
      Session* sess;
    public:
      Shioaji();
      virtual ~Shioaji();
      bool login(const std::string& person_id, const std::string& password);

 };
}