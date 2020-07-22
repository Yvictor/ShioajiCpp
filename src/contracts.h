#ifndef SHIOAJICPP_SRC_CONTRACTS_H_
#define SHIOAJICPP_SRC_CONTRACTS_H_
#include "shioaji.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace shioaji {

namespace contracts {
void to_json(json& j, const BaseContract& contract) {
  j = json{{"security_type", contract.security_type},
           {"exchange", contract.exchange},
           {"code", contract.code}};
}
void from_json(const json& j, BaseContract& contract) {
  j.at("security_type").get_to(contract.security_type);
  j.at("exchange").get_to(contract.exchange);
  j.at("code").get_to(contract.code);
}
NLOHMANN_JSON_SERIALIZE_ENUM(SecurityType, {
{Index, "IND"},
{Stock, "STK"},
{Futures, "FUT"},
{Options, "OPT"},
})
NLOHMANN_JSON_SERIALIZE_ENUM(Exchange, {
{TSE, "TSE"},
{OTC, "OTC"},
{OES, "OES"},
{TAIFEX, "TAIFEX"},
})
}  // namespace contracts

}

#endif //SHIOAJICPP_SRC_CONTRACTS_H_
