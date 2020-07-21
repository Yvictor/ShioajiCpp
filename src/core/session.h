#ifndef SHIOAJICPP_SRC_CORE_SESSION_H_
#define SHIOAJICPP_SRC_CORE_SESSION_H_
#include <string>
#include "sol.h"
#include "shioaji.h"

namespace shioaji {

class Session {
 private:
  std::string _token;
  CSol sol;
  void set_token(const std::string &token);
 public:
  Session(const std::string &host,
          const std::string &vpn,
          const std::string &user,
          const std::string &password,
          const std::string &clientname = "");
  Session(CSol sol);
  virtual ~Session();
  std::vector<account::Account> Login(const std::string &person_id, const std::string &password);
};

}
#endif //SHIOAJICPP_SRC_CORE_SESSION_H_
