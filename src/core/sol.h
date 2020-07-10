#ifndef SHIOAJICPP_LIBRARY_H
#define SHIOAJICPP_LIBRARY_H

#include <queue>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "libsolclient.h"
#include <nlohmann/json.hpp>

#include "core/spdlevel.h"
#include "core/solmsg.h"

using json = nlohmann::json;

typedef struct solApi_eventCallbackInfo {
  solApi_eventCallbackInfo(
      const solClient_session_eventCallbackInfo_pt &eventInfo_p);
  ~solApi_eventCallbackInfo();
  solClient_session_event_t sessionEvent;
  solClient_session_responseCode_t responseCode;
  const char *info_p;
} solApi_eventCallbackInfo_t, *solApi_eventCallbackInfo_pt;

class CSol {
 protected:
  friend void eventCallback(solClient_opaqueSession_pt,
                            solClient_session_eventCallbackInfo_pt, void *obj);
  friend solClient_rxMsgCallback_returnCode_t messageReceiveCallback(
      solClient_opaqueSession_pt, solClient_opaqueMsg_pt, void *obj);

 protected:
  std::queue<solClient_opaqueMsg_pt> msgQueue;
  std::queue<solApi_eventCallbackInfo_pt> eventQueue;
  std::mutex msgQueueMutex;
  std::mutex eventQueueMutex;
  std::condition_variable msgCond;
  std::condition_variable eventCond;
  std::thread msgThread;
  std::thread eventThread;

  bool msg_quit;
  bool event_quit;
  bool connected;

  std::uint64_t counter;

  ////////////////////
  solClient_opaqueContext_pt context_p;
  solClient_opaqueSession_pt session_p;
  std::function<int(SolMsg &)> requestMsgCallback;
  std::function<void(SolMsg &)> p2pMsgCallback;
  std::function<void(SolMsg &)> quoteMsgCallback;
  std::function<void(SolMsg &)> replyMsgCallback;
  std::function<void(solApi_eventCallbackInfo_pt)> LoopProcEventCallback;

 private:
  CSol(const CSol &);
  CSol &operator=(const CSol &);

  std::string _gen_corrid();

 public:
  CSol(std::function<int(SolMsg &)> requestMsgCallback,
       std::function<void(SolMsg &)> p2pMsgCallback,
       std::function<void(SolMsg &)> quoteMsgCallback,
       std::function<void(SolMsg &)> replyMsgCallback,
       std::function<void(solApi_eventCallbackInfo_pt)> LoopProcEventCallback);
  CSol();
  virtual ~CSol();
  CSol(CSol &&obj);
  CSol &operator=(CSol &&obj);

  solClient_rxMsgCallback_returnCode_t OnMessageReceived(
      solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p);
  void OnEventCallback(solClient_opaqueSession_pt opaqueSession_p,
                       solClient_session_eventCallbackInfo_pt eventInfo_p);
  void LoopProc();
  int LoopProcCallback(solClient_opaqueMsg_pt msg_p);

  void LoopProcEvent();

  int Connect(const char *host, const char *vpn, const char *user,
              const char *pass, const char *clientname = "");
  int DisConnect();
  void DestroySession();
  void Join();
  int GetMsgQueueSize();
  void SubscribeTopic(const char *topic);
  void UnSubscribeTopic(const char *topic);
  solClient_returnCode_t PublishTopic(solClient_opaqueMsg_pt msg_p);
  solClient_returnCode_t PublishTopic(SolMsg &msg);
  solClient_returnCode_t PublishTopic(const std::string &topic, const json &j,
                                      const std::string &format = "msgpack",
                                      bool elide = false);
  solClient_returnCode_t SendRequest(solClient_opaqueMsg_pt msg_p,
                                     solClient_opaqueMsg_pt &replyMsg_p,
                                     solClient_uint32_t timeout);
  std::string SendRequest(SolMsg &msg, solClient_opaqueMsg_pt &replyMsg_p,
                          solClient_uint32_t timeout);
  SolMsg SendRequest(const std::string &request_topic, const json &j,
                     solClient_uint32_t timeout,
                     const std::string &format = "msgpack");
  void Reply(solClient_opaqueMsg_pt, const char *replyTo,
             const char *correlationid, solClient_opaqueContainer_pt &);
  int Reply(solClient_opaqueMsg_pt, const char *reply2topic,
            const char *p2p_dest, const char *correlationid,
            solClient_opaqueContainer_pt &);
  solClient_returnCode_t Reply(solClient_opaqueMsg_pt request_msg_p,
                               solClient_opaqueMsg_pt reply_msg_p);
  solClient_returnCode_t Reply(SolMsg &reply_msg);
  solClient_returnCode_t Reply(const std::string &reply2topic,
                               const std::string &p2p,
                               const std::string &correlationid,
                               const json &response,
                               const std::string &format = "msgpack");

};

#endif //SHIOAJICPP_LIBRARY_H
