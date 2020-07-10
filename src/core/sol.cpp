#include "sol.h"

#include <utility>

solApi_eventCallbackInfo::solApi_eventCallbackInfo(
    const solClient_session_eventCallbackInfo_pt &eventInfo_p)
//: info_p(new char[strlen(eventInfo_p->info_p) + 1]{eventInfo_p->info_p})
{
  char *info_pl = new char[strlen(eventInfo_p->info_p) + 1];
  info_p = new char[strlen(eventInfo_p->info_p) + 1];
  memcpy(&sessionEvent, &eventInfo_p->sessionEvent,
         sizeof(eventInfo_p->sessionEvent));
  memcpy(&responseCode, &eventInfo_p->responseCode,
         sizeof(eventInfo_p->responseCode));
  strcpy(info_pl, eventInfo_p->info_p);
  info_p = info_pl;
  SPDLOG_DEBUG("solApi event copy constractor is called.");
}

solApi_eventCallbackInfo::~solApi_eventCallbackInfo() {
  SPDLOG_DEBUG("solApi event deconstractor is called.");
  delete[] info_p;
}

void eventCallback(solClient_opaqueSession_pt opaqueSession_p,
                   solClient_session_eventCallbackInfo_pt eventInfo_p,
                   void *obj) {
  (*(CSol *) obj).OnEventCallback(opaqueSession_p, eventInfo_p);
}

solClient_rxMsgCallback_returnCode_t messageReceiveCallback(
    solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p,
    void *obj) {
  return (*(CSol *) obj).OnMessageReceived(opaqueSession_p, msg_p);
}

CSol::CSol(
    std::function<int(SolMsg &)> requestMsgCallback,
    std::function<void(SolMsg &)> p2pMsgCallback,
    std::function<void(SolMsg &)> quoteMsgCallback,
    std::function<void(SolMsg &)> replyMsgCallback,
    std::function<void(solApi_eventCallbackInfo_pt)> LoopProcEventCallback)
    : msgThread(&CSol::LoopProc, this),
      eventThread(&CSol::LoopProcEvent, this),
      msg_quit(false),
      event_quit(false),
      connected(false),
      counter(0),
      context_p(nullptr),
      session_p(nullptr),
      requestMsgCallback(std::move(requestMsgCallback)),
      p2pMsgCallback(std::move(p2pMsgCallback)),
      quoteMsgCallback(std::move(quoteMsgCallback)),
      replyMsgCallback(std::move(replyMsgCallback)),
      LoopProcEventCallback(std::move(LoopProcEventCallback)) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_level(spdlog::level::debug);
#endif
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
}

CSol::CSol() : msgThread(&CSol::LoopProc, this),
               eventThread(&CSol::LoopProcEvent, this),
               msg_quit(false),
               event_quit(false),
               connected(false),
               counter(0),
               context_p(nullptr),
               session_p(nullptr),
               requestMsgCallback([&](SolMsg &msg) {
                 spdlog::debug(msg.dump());
                 return 0;
               }),
               p2pMsgCallback([&](SolMsg &msg) {
                 spdlog::debug(msg.dump());
                 return 0;
               }),
               quoteMsgCallback([&](SolMsg &msg) {
                 spdlog::debug(msg.dump());
                 return 0;
               }),

               replyMsgCallback([&](SolMsg &msg) {
                 spdlog::debug(msg.dump());
                 return 0;
               }),
               LoopProcEventCallback([&](solApi_eventCallbackInfo_pt event_p) {
                 spdlog::info("Response Code: {} | Event Code: {} | Info: {} | Event: {}",
                              event_p->responseCode, event_p->sessionEvent, event_p->info_p,
                              solClient_session_eventToString(event_p->sessionEvent)
                 );
               }) {
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_DEBUG
  spdlog::set_level(spdlog::level::debug);
#endif
  spdlog::set_pattern("[%H:%M:%S.%f] [%i] [%@:%!] [%L] [thread %t] %v");
}

CSol::~CSol() {
  this->DestroySession();
  this->Join();
}

CSol::CSol(CSol &&obj)
    : msgThread(std::move(obj.msgThread)),
      eventThread(std::move(obj.eventThread)) {
  SPDLOG_DEBUG("Move Construct is called");
}

CSol &CSol::operator=(CSol &&obj) {
  SPDLOG_DEBUG("Move Assignment is called");
  if (msgThread.joinable()) {
    msgThread.join();
  }
  if (eventThread.joinable()) {
    eventThread.join();
  }
  msgThread = std::move(obj.msgThread);
  eventThread = std::move(obj.eventThread);
  return *this;
}

int CSol::DisConnect() {
  if (session_p) {
    connected = false;
    if (connected) {
      SPDLOG_DEBUG("Solace Session Disconnected.");
      return solClient_session_disconnect(session_p);
    }
  }
  return SOLCLIENT_FAIL;
}

void CSol::DestroySession() {
  if (session_p) {
    this->DisConnect();
    solClient_session_destroy(&session_p);
    session_p = NULL;
    SPDLOG_DEBUG("Solace Session Destroy.");
  }
}

void CSol::Join() {
  if (msgThread.joinable()) {
    std::unique_lock<std::mutex> msgLock(msgQueueMutex);
    if (!msg_quit) {
      msg_quit = true;
      msgCond.notify_one();
    }
    msgLock.unlock();
    msgThread.join();
    SPDLOG_DEBUG("Msg Thread Joined.");
  }
  if (eventThread.joinable()) {
    std::unique_lock<std::mutex> eventLock(eventQueueMutex);
    if (!event_quit) {
      event_quit = true;
      eventCond.notify_one();
    }
    eventLock.unlock();
    eventThread.join();
    SPDLOG_DEBUG("Event Thread Joined.");
  }
}

solClient_rxMsgCallback_returnCode_t CSol::OnMessageReceived(
    solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p) {
  std::lock_guard<std::mutex> lockGuard(msgQueueMutex);
  msgQueue.push(msg_p);
  msgCond.notify_one();
  return SOLCLIENT_CALLBACK_TAKE_MSG;
}

void CSol::OnEventCallback(solClient_opaqueSession_pt opaqueSession_p,
                           solClient_session_eventCallbackInfo_pt eventInfo_p) {
  auto copiedEventInfo = new solApi_eventCallbackInfo(eventInfo_p);
  std::lock_guard<std::mutex> lockGuard(eventQueueMutex);
  eventQueue.push(copiedEventInfo);
  eventCond.notify_one();
}

void CSol::LoopProc() {
  std::unique_lock<std::mutex> msgLock(msgQueueMutex);
  msgCond.wait(msgLock);
  while (!msg_quit) {
    while (!msgQueue.empty()) {
      solClient_opaqueMsg_pt msg_p = msgQueue.front();
      msgQueue.pop();
      msgLock.unlock();
      int KeepOrZero = this->LoopProcCallback(msg_p);
      if (KeepOrZero == 0) {
        // solClient_msg_free(&msg_p);
      }
      msgLock.lock();
    }
    if (!msg_quit) {
      msgCond.wait(msgLock);
    }
  }
}

int CSol::LoopProcCallback(solClient_opaqueMsg_pt msg_p) {
  SolMsg msg(msg_p);
  if (msg.isRequest()) {
    this->requestMsgCallback(msg);
  } else if (msg.isReply()) {
    this->replyMsgCallback(msg);
  } else if (msg.isP2P()) {
    this->p2pMsgCallback(msg);
  } else {
    this->quoteMsgCallback(msg);
  }
  return 0;
}

void CSol::LoopProcEvent() {
  std::unique_lock<std::mutex> eventLock(eventQueueMutex);
  eventCond.wait(eventLock);
  SPDLOG_DEBUG("Thread Loop Started.");
  while (!event_quit) {
    SPDLOG_DEBUG("Entry Thread Loop, event_quit: {}", event_quit);
    while (!eventQueue.empty()) {
      SPDLOG_DEBUG("Event Queue Deque");
      solApi_eventCallbackInfo_pt eventInfo_p = eventQueue.front();
      eventQueue.pop();
      SPDLOG_DEBUG("Event Unlocking");
      eventLock.unlock();
      SPDLOG_DEBUG("Event Unlocked");
      this->LoopProcEventCallback(eventInfo_p);
      delete eventInfo_p;
      SPDLOG_DEBUG("Event Locking");
      eventLock.lock();
      SPDLOG_DEBUG("Event Locked");
    }
    if (!event_quit) {
      SPDLOG_DEBUG("Event Thread Wait.");
      eventCond.wait(eventLock);
    }
  }
  SPDLOG_DEBUG("Event Thread Exiting.");
}

int CSol::Connect(const char *host, const char *vpn, const char *user,
                  const char *pass, const char *clientname) {
  const char *sessionProps[] = {
      SOLCLIENT_SESSION_PROP_HOST,
      host,
      SOLCLIENT_SESSION_PROP_VPN_NAME,
      vpn,
      SOLCLIENT_SESSION_PROP_USERNAME,
      user,
      SOLCLIENT_SESSION_PROP_PASSWORD,
      pass,
      SOLCLIENT_SESSION_PROP_CLIENT_NAME,
      clientname,
      SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL,
      (NULL != strstr(host, ":55555") || NULL != strstr(host, "http://")) ? "0"
                                                                          : "1",
      SOLCLIENT_SESSION_PROP_GENERATE_SEQUENCE_NUMBER,
      SOLCLIENT_PROP_ENABLE_VAL,
      SOLCLIENT_SESSION_PROP_GENERATE_SEND_TIMESTAMPS,
      SOLCLIENT_PROP_ENABLE_VAL,
      SOLCLIENT_SESSION_PROP_GENERATE_RCV_TIMESTAMPS,
      SOLCLIENT_PROP_ENABLE_VAL,
      SOLCLIENT_SESSION_PROP_TCP_NODELAY,
      SOLCLIENT_PROP_ENABLE_VAL,
      SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS,
      SOLCLIENT_PROP_ENABLE_VAL,
      SOLCLIENT_SESSION_PROP_CONNECT_TIMEOUT_MS,
      "3000",
      SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES,
      "3",
      SOLCLIENT_SESSION_PROP_KEEP_ALIVE_INT_MS,
      "3000",
      SOLCLIENT_SESSION_PROP_KEEP_ALIVE_LIMIT,
      "6",
      NULL};

  solClient_context_createFuncInfo_t contextFuncInfo =
      SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;
  solClient_session_createFuncInfo_t sessionFuncInfo =
      SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

  sessionFuncInfo.rxMsgInfo.callback_p = messageReceiveCallback;
  sessionFuncInfo.rxMsgInfo.user_p = this;

  sessionFuncInfo.eventInfo.callback_p = eventCallback;
  sessionFuncInfo.eventInfo.user_p = this;

  solClient_context_create(SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                           &context_p, &contextFuncInfo,
                           sizeof(contextFuncInfo));

  solClient_session_create((char **) sessionProps, context_p, &session_p,
                           &sessionFuncInfo, sizeof(sessionFuncInfo));
  solClient_returnCode_t ret = solClient_session_connect(session_p);

  if (ret != SOLCLIENT_OK) {
    printf(__FILE__ "(%d): connected failed, return %s\n", __LINE__,
           solClient_returnCodeToString(ret));
  }
  connected = true;
  return ret;

  /*
  // check if support compress
  solClient_field_t field;
  solClient_session_getCapability( session_p,
  SOLCLIENT_SESSION_CAPABILITY_COMPRESSION, &field, sizeof(field) );

  if( field.type==SOLCLIENT_BOOL && field.value.boolean )
  {
      printf( __FILE__ "(%d): CAPABILITY_COMPRESSION=%d\n", __LINE__,
  field.value.boolean );

      const char*sessionCompressProps[] = {
  SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL, "1", NULL };
      solClient_session_modifyProperties( session_p, (char**)
  sessionCompressProps );
  }
*/
}

int CSol::GetMsgQueueSize() {
  std::lock_guard<std::mutex> lockGuard(msgQueueMutex);
  int queueSize = msgQueue.size();
  return queueSize;
}

std::string CSol::_gen_corrid() {
  counter += 1;
  return fmt::format("c{}", counter);
}

void CSol::SubscribeTopic(const char *topic) {
  solClient_session_topicSubscribeExt(
      session_p, SOLCLIENT_SUBSCRIBE_FLAGS_REQUEST_CONFIRM, topic);
}

void CSol::UnSubscribeTopic(const char *topic) {
  solClient_session_topicUnsubscribeExt(
      session_p, SOLCLIENT_SUBSCRIBE_FLAGS_REQUEST_CONFIRM, topic);
}

solClient_returnCode_t CSol::PublishTopic(solClient_opaqueMsg_pt msg_p) {
  if (msg_p) {
    solClient_returnCode_t ret = solClient_session_sendMsg(session_p, msg_p);
    // solClient_msg_free(&msg_p);
    if (ret != SOLCLIENT_OK) {
      SPDLOG_ERROR("{}", solClient_returnCodeToString(ret));
    }
    return ret;
  }
  return SOLCLIENT_NOT_FOUND;
}

solClient_returnCode_t CSol::PublishTopic(SolMsg &msg) {
  return this->PublishTopic(msg.ptr());
}

solClient_returnCode_t CSol::PublishTopic(const std::string &topic,
                                          const json &j,
                                          const std::string &format,
                                          bool elide) {
  SolMsg msg(topic, j, format, elide);
  return this->PublishTopic(msg);
}

void CSol::Reply(solClient_opaqueMsg_pt msg_req, const char *replyTo,
                 const char *correlationid,
                 solClient_opaqueContainer_pt &container_p) {
  if (container_p) {
    solClient_opaqueMsg_pt msg_p = nullptr;
    solClient_msg_alloc(&msg_p);

    solClient_destination_t destination;
    destination.destType = SOLCLIENT_TOPIC_DESTINATION;
    destination.dest = replyTo;
    solClient_msg_setDestination(msg_p, &destination, sizeof(destination));

    solClient_msg_setReplyTo(msg_p, &destination, sizeof(destination));
    solClient_msg_setCorrelationId(msg_p, correlationid);

    if (msg_p) {
      solClient_msg_setDeliveryMode(msg_p, SOLCLIENT_DELIVERY_MODE_DIRECT);
      // solClient_msg_setDestination( msg_p, &destination,
      // sizeof(destination));

      solClient_msg_setBinaryAttachmentContainer(msg_p, container_p);

      // pthread_mutex_lock( &mut );
      solClient_returnCode_t ret =
          solClient_session_sendReply(session_p, msg_req, msg_p);
      // pthread_mutex_unlock( &mut );

      if (ret != SOLCLIENT_OK) {
        printf(__FILE__ "(%d): session_sendMsg failed, err=%s\n", __LINE__,
               solClient_returnCodeToString(ret));
      }

      solClient_msg_free(&msg_p);
    }
  }
}

int CSol::Reply(solClient_opaqueMsg_pt msg_req, const char *reply2topic,
                const char *p2p_dest, const char *correlationid,
                solClient_opaqueContainer_pt &container_p) {
  if (container_p) {
    solClient_opaqueMsg_pt msg_p = nullptr;
    solClient_msg_alloc(&msg_p);

    solClient_destination_t destination;
    solClient_destination_t reply2dest;
    destination.destType = SOLCLIENT_TOPIC_DESTINATION;
    destination.dest = p2p_dest;
    reply2dest.destType = SOLCLIENT_TOPIC_DESTINATION;
    reply2dest.dest = reply2topic;
    solClient_msg_setDestination(msg_p, &destination, sizeof(destination));
    solClient_msg_setReplyTo(msg_p, &reply2dest, sizeof(reply2dest));

    solClient_msg_setCorrelationId(msg_p, correlationid);

    if (msg_p) {
      solClient_msg_setDeliveryMode(msg_p, SOLCLIENT_DELIVERY_MODE_DIRECT);

      solClient_msg_setBinaryAttachmentContainer(msg_p, container_p);

      // pthread_mutex_lock( &mut );
      solClient_returnCode_t ret =
          solClient_session_sendReply(session_p, msg_req, msg_p);
      // pthread_mutex_unlock( &mut );

      if (ret != SOLCLIENT_OK) {
        printf(__FILE__ "(%d): session_sendMsg failed, err=%s\n", __LINE__,
               solClient_returnCodeToString(ret));
      }

      solClient_msg_free(&msg_p);
      return ret;
    }
    return SOLCLIENT_NOT_READY;
  }
  return SOLCLIENT_NOT_READY;
}

solClient_returnCode_t CSol::Reply(solClient_opaqueMsg_pt request_msg_p,
                                   solClient_opaqueMsg_pt reply_msg_p) {
  if (reply_msg_p) {
    solClient_returnCode_t ret =
        solClient_session_sendReply(session_p, request_msg_p, reply_msg_p);
    if (ret != SOLCLIENT_OK) {
      SPDLOG_ERROR("{}", solClient_returnCodeToString(ret));
    }
    return ret;
  }
  return SOLCLIENT_NOT_READY;
}

solClient_returnCode_t CSol::Reply(SolMsg &reply_msg) {
  return this->Reply(nullptr, reply_msg.ptr());
}

solClient_returnCode_t CSol::Reply(const std::string &reply2topic,
                                   const std::string &p2p,
                                   const std::string &correlationid,
                                   const json &response,
                                   const std::string &format) {
  SolMsg msg(p2p, response, format);
  msg.setReplyToDestination(
      Destination(reply2topic, SOLCLIENT_TOPIC_DESTINATION));
  msg.setCorrlationId(correlationid);
  return this->Reply(msg);
}

solClient_returnCode_t CSol::SendRequest(solClient_opaqueMsg_pt msg_p,
                                         solClient_opaqueMsg_pt &replyMsg_p,
                                         solClient_uint32_t timeout) {
  if (msg_p) {
    solClient_returnCode_t rc =
        solClient_session_sendRequest(session_p, msg_p, &replyMsg_p, timeout);
    if (rc != SOLCLIENT_OK && rc != SOLCLIENT_IN_PROGRESS) {
      SPDLOG_ERROR("{}", solClient_returnCodeToString(rc));
    }
    return rc;
  }
  SPDLOG_ERROR("msg_p not valid at {:p}", msg_p);
  return SOLCLIENT_NOT_FOUND;
}

std::string CSol::SendRequest(SolMsg &msg, solClient_opaqueMsg_pt &replyMsg_p,
                              solClient_uint32_t timeout) {
  SPDLOG_DEBUG("gen corrid.");
  std::string corrid;
  if (!timeout) {
    corrid = this->_gen_corrid();
    msg.setCorrlationId(corrid);
  }
  SPDLOG_DEBUG("gen corrid spend.");
  msg.setDeliveryToOne(true);
  this->SendRequest(msg.ptr(), replyMsg_p, timeout);
  return corrid;
}

SolMsg CSol::SendRequest(const std::string &request_topic, const json &j,
                         solClient_uint32_t timeout,
                         const std::string &format) {
  SolMsg msg(request_topic, j, format);
  solClient_opaqueMsg_pt replyMsg_p = nullptr;
  std::string corrid = this->SendRequest(msg, replyMsg_p, timeout);
  SolMsg reply(replyMsg_p);
  reply.setCorrlationId(corrid);
  return reply;
}