#ifndef SHIOAJICPP_SRC_ACCOUNT_H_
#define SHIOAJICPP_SRC_ACCOUNT_H_
#include "shioaji.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace shioaji{

namespace account {
void to_json(json& j, const Account& account) {
  j = json{{"account_type", account.account_type},
           {"person_id", account.person_id},
           {"broker_id", account.broker_id},
           {"account_id", account.account_id},
           {"signed", account._signed}};
}
void from_json(const json& j, Account& account) {
  j.at("account_type").get_to(account.account_type);
  j.at("person_id").get_to(account.person_id);
  j.at("broker_id").get_to(account.broker_id);
  j.at("account_id").get_to(account.account_id);
  j.at("signed").get_to(account._signed);
}

NLOHMANN_JSON_SERIALIZE_ENUM(AccountType, {{Stock, "S"}, {Futures, "F"}, {H, "H"}, {InValid, nullptr}})
}  // namespace account

}
#endif //SHIOAJICPP_SRC_ACCOUNT_H_
