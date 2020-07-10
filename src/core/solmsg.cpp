#include <string>
#include "solmsg.h"
Bytes::Bytes() : bytes_p(NULL), size(0) {};

Destination::Destination(const std::string &dest,
                         solClient_destinationType_t destType)
    : dest(dest), destType(destType) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
  spdlog::set_level(spdlog::level::debug);
#endif
};

Destination::Destination(solClient_destination_t dest)
    : dest(dest.dest), destType(dest.destType) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
  spdlog::set_level(spdlog::level::debug);
#endif
  SPDLOG_DEBUG("dest_t construct is called");
};

solClient_destination_t Destination::to_solClient_destination_t() const {
  solClient_destination_t d;
  d.dest = dest.c_str();
  d.destType = destType;
  return d;
};

SolMsg::SolMsg() : msg_p(nullptr), container_p(nullptr) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
  spdlog::set_level(spdlog::level::debug);
#endif
  if (SOLCLIENT_OK != solClient_msg_alloc(&msg_p)) {
    solClient_errorInfo_pt err_info = solClient_getLastErrorInfo();
    SPDLOG_ERROR("SolMsg alloc failed, error: {}", err_info->errorStr);
  }
}

SolMsg::SolMsg(solClient_opaqueMsg_pt msg_p) : msg_p(msg_p), container_p(NULL) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
  spdlog::set_level(spdlog::level::debug);
#endif
}

SolMsg::SolMsg(const std::string &topic, const json &j,
               const std::string &content_type, bool elide, DeliveryMode mode)
    : msg_p(nullptr), container_p(nullptr) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
  spdlog::set_level(spdlog::level::debug);
#endif
  if (SOLCLIENT_OK != solClient_msg_alloc(&msg_p)) {
    solClient_errorInfo_pt err_info = solClient_getLastErrorInfo();
    SPDLOG_ERROR("SolMsg alloc failed, error: {}", err_info->errorStr);
  } else {
    if (content_type == "msgpack") {
      this->setMsgpackBody(j);
    } else if (content_type == "json") {
      this->setJsonBody(j);
    }
    this->setDestination(Destination(topic, SOLCLIENT_TOPIC_DESTINATION));
    this->setDeliveryMode(mode);
    this->setElidingEligible(elide);
  }
}

SolMsg::SolMsg(SolMsg &&msg) noexcept: msg_p(nullptr), container_p(nullptr) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
  spdlog::set_level(spdlog::level::debug);
#endif
  std::swap(msg.msg_p, msg_p);
  std::swap(msg.container_p, container_p);
  SPDLOG_DEBUG("Move Constructor called.");
}

SolMsg::~SolMsg() {
  SPDLOG_DEBUG("SolMsg Deconstrcutor called. msg_p ({}), container_p ({})",
               msg_p, container_p);
  if (container_p) {
    solClient_container_closeMapStream(&container_p);
  }
  if (msg_p) {
    solClient_msg_free(&msg_p);
    msg_p = nullptr;
  }
}

solClient_opaqueMsg_pt SolMsg::ptr() { return msg_p; }

std::string SolMsg::dump() {
  char c[4096] = {0};
  solClient_msg_dump(msg_p, c, sizeof(c));
  std::string s = c;
  return s;
}
solClient_returnCode_t SolMsg::setDeliveryMode(DeliveryMode mode) {
  return solClient_msg_setDeliveryMode(msg_p, mode);
}

DeliveryMode SolMsg::getDeliveryMode() {
  solClient_uint32_t mode;
  solClient_msg_getDeliveryMode(msg_p, &mode);
  return DeliveryMode(mode);
}

solClient_returnCode_t SolMsg::setBinaryAttachment(const std::string &str) {
  return solClient_msg_setBinaryAttachment(msg_p, str.c_str(), str.size());
}

solClient_returnCode_t SolMsg::setBinaryAttachment(const void *bytes_p,
                                                   size_t size) {
  return solClient_msg_setBinaryAttachment(msg_p, bytes_p, size);
}

std::string SolMsg::getBinaryAttachmentString() {
  solClient_uint32_t size;
  void *bytes_p;
  solClient_msg_getBinaryAttachmentPtr(msg_p, &bytes_p, &size);
  std::string s(static_cast<const char *>(bytes_p),
                static_cast<const char *>(bytes_p) + size);
  return s;
}

Bytes SolMsg::getBinaryAttachment() {
  Bytes bytes = Bytes();
  solClient_msg_getBinaryAttachmentPtr(msg_p, &bytes.bytes_p, &bytes.size);
  return bytes;
}

solClient_returnCode_t SolMsg::createContainerMap(char *buff, size_t size) {
  if (!container_p) {
    SPDLOG_DEBUG("create container");
    solClient_returnCode_t rc =
        solClient_container_createMap(&container_p, buff, size);
    return rc;
  }
  return SOLCLIENT_OK;
}
solClient_returnCode_t SolMsg::addContainerString(char *buff, size_t size,
                                                  const std::string &key,
                                                  const std::string &value) {
  this->createContainerMap(buff, size);
  return solClient_container_addString(container_p, value.c_str(), key.c_str());
}

solClient_returnCode_t SolMsg::attachmentContainerMap() {
  return solClient_msg_setBinaryAttachmentContainer(msg_p, container_p);
}

solClient_opaqueContainer_pt SolMsg::getBinaryAttachmentMap() {
  // solClient_opaqueContainer_pt container_p = NULL;
  solClient_msg_getBinaryAttachmentMap(msg_p, &container_p);
  return container_p;
}

std::vector<std::uint8_t> SolMsg::getBinaryAttachmentVector() {
  solClient_uint32_t size;
  void *bytes_p;
  solClient_msg_getBinaryAttachmentPtr(msg_p, &bytes_p, &size);
  std::vector<std::uint8_t> vec(static_cast<std::uint8_t *>(bytes_p),
                                static_cast<std::uint8_t *>(bytes_p) + size);
  return vec;
}

json SolMsg::getJsonBody() {
  json j;
  std::string content_type = this->getContentType();
  if (content_type == "json" || content_type == "application/json") {
    std::vector<std::uint8_t> barr = this->getBinaryAttachmentVector();
    j = json::parse(barr);
  }
  return j;
}

solClient_returnCode_t SolMsg::setJsonBody(const json &j) {
  this->setContentType("json");
  return this->setBinaryAttachment(j.dump());
}

json SolMsg::getMsgpackBody() {
  json j;
  if (this->getContentType() == "msgpack") {
    std::vector<std::uint8_t> barr = this->getBinaryAttachmentVector();
    j = json::from_msgpack(barr);
  }
  return j;
}

solClient_returnCode_t SolMsg::setMsgpackBody(const json &j) {
  this->setContentType("msgpack");
  std::vector<std::uint8_t> barr = json::to_msgpack(j);
  return this->setBinaryAttachment(barr.data(), barr.size());
}

solClient_returnCode_t SolMsg::setContentType(const std::string &string) {
  return setMetaContentType(msg_p, string.c_str());
}

std::string SolMsg::getContentType() {
  std::string content_type;
  const char *str;
  solClient_returnCode rc;
  rc = getMetaContentType(msg_p, &str);
  if (rc == SOLCLIENT_OK) {
    content_type.assign(str);
  } else if (rc == SOLCLIENT_NOT_FOUND) {
    rc = solClient_msg_getHttpContentType(msg_p, &str);
    if (rc == SOLCLIENT_OK) {
      content_type.assign(str);
    }
  }
  return content_type;
}

solClient_returnCode_t SolMsg::addMetaData(const std::string &key,
                                           const std::string &value) {
  solClient_opaqueContainer_pt meta_p;
  solClient_returnCode rc;
  size_t meta_size = 64;
  rc = solClient_msg_createUserPropertyMap(msg_p, &meta_p, meta_size);
  if (rc == SOLCLIENT_OK) {
    rc = solClient_container_addString(meta_p, value.c_str(), key.c_str());
  }
  return rc;
}

std::string SolMsg::getMetaData(const std::string &key) {
  solClient_opaqueContainer_pt meta_p;
  std::string meta("");
  const char *str;
  solClient_returnCode rc;
  rc = solClient_msg_getUserPropertyMap(msg_p, &meta_p);
  if (rc == SOLCLIENT_OK) {
    rc = solClient_container_getStringPtr(meta_p, &str, key.c_str());
    if (rc == SOLCLIENT_OK) {
      meta.assign(str);
    }
  }
  return meta;
}

solClient_returnCode_t setMetaContentType(solClient_opaqueMsg_pt msg_p,
                                          const char *content_type) {
  solClient_opaqueContainer_pt meta_p;
  solClient_returnCode rc;
  size_t meta_size = 64;
  rc = solClient_msg_createUserPropertyMap(msg_p, &meta_p, meta_size);
  if (rc == SOLCLIENT_OK) {
    rc = solClient_container_addString(meta_p, content_type, "ct");
  }
  return rc;
}

solClient_returnCode_t getMetaContentType(solClient_opaqueMsg_pt msg_p,
                                          const char **content_type_p) {
  solClient_opaqueContainer_pt meta_p;
  solClient_returnCode rc;
  rc = solClient_msg_getUserPropertyMap(msg_p, &meta_p);
  if (rc == SOLCLIENT_OK) {
    rc = solClient_container_getStringPtr(meta_p, content_type_p, "ct");
  }
  return rc;
}

solClient_returnCode_t SolMsg::setDestination(solClient_destination_t d) {
  return solClient_msg_setDestination(msg_p, &d, sizeof(d));
}

solClient_returnCode_t SolMsg::setDestination(const Destination &dest) {
  solClient_destination_t d = dest.to_solClient_destination_t();
  return solClient_msg_setDestination(msg_p, &d, sizeof(d));
}

Destination SolMsg::getDestination() const {
  solClient_destination_t dest_p;
  dest_p.dest = "";
  solClient_msg_getDestination(msg_p, &dest_p, sizeof(dest_p));
  return dest_p;
}

solClient_returnCode_t SolMsg::setAsReply(bool reply) {
  return solClient_msg_setAsReplyMsg(msg_p, reply);
}

bool SolMsg::isReply() { return solClient_msg_isReplyMsg(msg_p); }

solClient_returnCode_t SolMsg::setReplyToDestination(const Destination &dest) {
  solClient_destination_t d = dest.to_solClient_destination_t();
  return solClient_msg_setReplyTo(msg_p, &d, sizeof(d));
}

Destination SolMsg::getReplyToDestination() {
  solClient_destination_t dest_p;
  dest_p.dest = "";
  solClient_msg_getReplyTo(msg_p, &dest_p, sizeof(dest_p));
  return dest_p;
}

bool SolMsg::isRequest() {
  solClient_destination_t dest_p;
  solClient_returnCode_t rc =
      solClient_msg_getReplyTo(msg_p, &dest_p, sizeof(dest_p));
  return (!this->isReply() && rc == SOLCLIENT_OK);
}

solClient_returnCode_t SolMsg::setElidingEligible(bool elide) {
  return solClient_msg_setElidingEligible(msg_p, elide);
}
bool SolMsg::isElidingEligible() {
  return solClient_msg_isElidingEligible(msg_p);
}

bool SolMsg::isP2P() const {
  Destination dest = this->getDestination();
  return (dest.dest.rfind("#P2P", 0) == 0);
}

std::string SolMsg::getCorrlationId() {
  const char *corrid;
  solClient_msg_getCorrelationId(msg_p, &corrid);
  return std::string(corrid);
}

solClient_returnCode_t SolMsg::setCorrlationId(const std::string &corrid) {
  return solClient_msg_setCorrelationId(msg_p, corrid.c_str());
}

solClient_returnCode_t SolMsg::setDeliveryToOne(bool to_one) {
  return solClient_msg_setDeliverToOne(msg_p, to_one);
}