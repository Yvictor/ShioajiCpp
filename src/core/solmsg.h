//
// Created by 011295 on 2020/5/25.
//

#ifndef SHIOAJICPP_SRC_CORE_SOLMSG_H_
#define SHIOAJICPP_SRC_CORE_SOLMSG_H_

#include "libsolclient.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class SolMsg {
 private:
  solClient_opaqueMsg_pt msg_p;
 public:
  SolMsg();
  solClient_opaqueMsg_pt ptr();
  solClient_returnCode_t setBinaryAttachment(const std::string str);
  std::string getBinaryAttachmentString();
};

#endif //SHIOAJICPP_SRC_CORE_SOLMSG_H_
