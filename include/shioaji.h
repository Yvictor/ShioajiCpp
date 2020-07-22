#include <string>
#include <vector>
#ifndef SHIOAJICPP_INCLUDE_SHIOAJI_H_
#define SHIOAJICPP_INCLUDE_SHIOAJI_H_

namespace shioaji {
namespace account {
enum AccountType { Stock, Futures, H, InValid };
struct Account {
  AccountType account_type;
  std::string person_id;
  std::string broker_id;
  std::string account_id;
  bool _signed;
};
}

namespace contracts {
enum SecurityType { Index, Stock, Futures, Options };
enum Exchange { TSE, OTC, OES, TAIFEX };

struct BaseContract {
  SecurityType security_type;
  Exchange exchange;
  std::string code;
};

struct Stock : BaseContract {
  SecurityType security_type = SecurityType::Stock;
  // limit_up : float = 0.0 limit_down : float = 0.0 reference : float = 0.0
};

}  // namespace contracts

class Session;
class Shioaji {
 private:
  Session *sess;
 public:
  Shioaji(bool simulation = false);
  Shioaji(Session * session);
  virtual ~Shioaji();
  std::vector<account::Account> login(const std::string &person_id, const std::string &password);
  std::vector<contracts::BaseContract> fetch_contracts();

};
}
#endif //SHIOAJICPP_INCLUDE_SHIOAJI_H_
