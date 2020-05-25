//
// Created by 011295 on 2020/5/25.
//

#include <string>
#include "solmsg.h"
SolMsg::SolMsg() : msg_p(NULL){
  if (SOLCLIENT_OK != solClient_msg_alloc(&msg_p)){
    solClient_errorInfo_pt error_info_pt = solClient_getLastErrorInfo();
//    std::cout << error_info_pt->errorStr << std::endl;
  }
}
solClient_opaqueMsg_pt SolMsg::ptr() {
  return msg_p;
}
solClient_returnCode_t SolMsg::setBinaryAttachment(const std::string str) {
  return solClient_msg_setBinaryAttachment(msg_p, str.c_str(), str.size());
}
std::string SolMsg::getBinaryAttachmentString() {
  solClient_uint32_t size;
  void * bytes_p;
  solClient_msg_getBinaryAttachmentPtr(msg_p, &bytes_p, &size);
  return std::string(static_cast<const char *>(bytes_p), static_cast<const char *>(bytes_p)+size);
}
