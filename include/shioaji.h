#include <string>
#ifndef SHIOAJICPP_INCLUDE_SHIOAJI_H_
#define SHIOAJICPP_INCLUDE_SHIOAJI_H_

namespace shioaji {
namespace account {
  enum AccountType {Stock, Futures, H, InValid};
  struct Account {
    AccountType account_type;
    std::string person_id;
    std::string broker_id;
    std::string account_id;
    bool _signed;
  };
}
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
#endif //SHIOAJICPP_INCLUDE_SHIOAJI_H_
