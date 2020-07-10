#ifndef SHIOAJICPP_SRC_CORE_SOLMSG_H_
#define SHIOAJICPP_SRC_CORE_SOLMSG_H_
#include <iostream>

#include "libsolclient.h"
#include <nlohmann/json.hpp>

#include "core/spdlevel.h"

using json = nlohmann::json;
enum DeliveryMode {
  Direct = SOLCLIENT_DELIVERY_MODE_DIRECT,
  Persistent = SOLCLIENT_DELIVERY_MODE_PERSISTENT,
  NonPersistent = SOLCLIENT_DELIVERY_MODE_NONPERSISTENT,
};

class Bytes {
 public:
  void *bytes_p;
  solClient_uint32_t size;
  Bytes();
};

class Destination {
 public:
  Destination(const std::string &dest, solClient_destinationType_t destType);
  Destination(solClient_destination_t dest);
  // Destination &operator=(solClient_destination_t &&dest_t);
  // Destination &operator=(const solClient_destination_t &dest_t);
  solClient_destination_t to_solClient_destination_t() const;
  std::string dest;
  solClient_destinationType_t destType;
};

class SolMsg {
 private:
  solClient_opaqueMsg_pt msg_p;
  solClient_opaqueContainer_pt container_p;
  // Destination reply2;
  SolMsg &operator=(const SolMsg &);
  SolMsg &operator=(SolMsg &&);
  SolMsg(const SolMsg &);

 public:
  SolMsg();
  SolMsg(solClient_opaqueMsg_pt msg_p);
  SolMsg(const std::string &topic, const json &j,
         const std::string &content_type = "msgpack", bool elide = false,
         DeliveryMode mode = DeliveryMode::Direct);
  SolMsg(SolMsg &&) noexcept;
  ~SolMsg();
  solClient_opaqueMsg_pt ptr();
  std::string dump();
  solClient_returnCode_t setDestination(solClient_destination_t dest);
  solClient_returnCode_t setDestination(const Destination &dest);
  Destination getDestination() const;
  bool isReply();
  solClient_returnCode_t setAsReply(bool reply);
  solClient_returnCode_t setReplyToDestination(const Destination &dest);
  Destination getReplyToDestination();
  bool isRequest();
  solClient_returnCode_t setContentType(const std::string &str);
  std::string getContentType();
  solClient_returnCode_t setDeliveryMode(
      DeliveryMode mode = DeliveryMode::Direct);
  DeliveryMode getDeliveryMode();
  solClient_returnCode_t setBinaryAttachment(const std::string &str);
  solClient_returnCode_t setBinaryAttachment(const void *bytes_p, size_t size);
  std::string getBinaryAttachmentString();
  Bytes getBinaryAttachment();
  solClient_opaqueContainer_pt getBinaryAttachmentMap();
  solClient_returnCode_t createContainerMap(char *buff, size_t size);
  solClient_returnCode_t addContainerString(char *buff, size_t size,
                                            const std::string &key,
                                            const std::string &value);
  solClient_returnCode_t attachmentContainerMap();
  std::vector<std::uint8_t> getBinaryAttachmentVector();
  json getJsonBody();
  solClient_returnCode_t setJsonBody(const json &j);
  json getMsgpackBody();
  solClient_returnCode_t setMsgpackBody(const json &j);
  solClient_returnCode_t addMetaData(const std::string &key,
                                     const std::string &value);
  std::string getMetaData(const std::string &key);
  solClient_returnCode_t setElidingEligible(bool elide);
  bool isElidingEligible();
  bool isP2P() const;
  std::string getCorrlationId();
  solClient_returnCode_t setCorrlationId(const std::string &corrid);
  solClient_returnCode_t setDeliveryToOne(bool to_one);
  // bool isDeliveryToOne();
};

solClient_returnCode_t getMetaContentType(solClient_opaqueMsg_pt msg_p,
                                          const char **content_type_p);
solClient_returnCode_t setMetaContentType(solClient_opaqueMsg_pt msg_p,
                                          const char *content_type);

#endif //SHIOAJICPP_SRC_CORE_SOLMSG_H_
