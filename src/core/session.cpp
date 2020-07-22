#include "session.h"
#include "account.h"
#include "contracts.h"

namespace shioaji {

Session::Session(const std::string &host,
                 const std::string &vpn,
                 const std::string &user,
                 const std::string &password,
                 const std::string &clientname) : sol() {
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
  solClient_initialize(SOLCLIENT_LOG_WARNING, nullptr);
  sol.Connect(host.c_str(),
              vpn.c_str(),
              user.c_str(),
              password.c_str(),
              clientname.c_str());
}

//Session::Session(CSol sol) : sol(std::move(sol)){
//  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
//}

Session::~Session() {}

void Session::set_token(const std::string &token) {
  this->_token = token;
}

std::vector<account::Account> Session::Login(const std::string &person_id,
                                             const std::string &password) {
  json payload;
  payload["person_id"] = person_id;
  payload["password"] = password;
  SolMsg respMsg = sol.SendRequest("api/v1/auth/login", payload, 5000);
  json body = respMsg.getMsgpackBody();
  std::vector<account::Account> accounts;
  if (body["status"]["status_code"].get<int>() == 200) {
    set_token(body["response"]["token"]);
    for (auto &element: body["response"]["accounts"]) {
      account::Account account = element;
      accounts.push_back(account);
    }
  } else {
    spdlog::error(body.dump());
  }
  return accounts;
}

std::vector<contracts::BaseContract> Session::FetchStockContracts() {
  std::vector<contracts::BaseContract> cs;
  json payload;
  if (this->_token != "") {
    payload["token"] = this->_token;
    payload["security_type"] = contracts::SecurityType::Stock;
    spdlog::info("downloading...");
    SolMsg respMsg = sol.SendRequest("api/v1/data/contracts", payload, 30000);
    json body = respMsg.getMsgpackBody();
    spdlog::info("downloaded.");
    if (body["status"]["status_code"].get<int>() == 200) {
      for (auto &element: body["response"]["contracts"]) {
        contracts::BaseContract contract = element;
        cs.push_back(contract);
      }
      spdlog::info("parsed.");
    } else {
      spdlog::error(body.dump());
    }
  } else {
    spdlog::error("Please login first.");
  }
  return cs;
}

}