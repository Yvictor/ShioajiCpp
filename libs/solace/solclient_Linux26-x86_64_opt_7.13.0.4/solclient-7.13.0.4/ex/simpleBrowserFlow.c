/** @example ex/simpleBrowserFlow.c
*/

/*
* This sample shows how to create and use browser flows.
* It demonstrates:
*    - creating a browser flow,
*    - browsing messages spooled on Endpoints without removing them.
*    - selectively removing messages from the persistent store of an Endpoint.  
*
* This sample requires that a durable Queue called 'my_sample_queue' be provisioned on the 
* appliance with at least 'Consume' permissions. 
*
* Copyright 2007-2020 Solace Corporation. All rights reserved.
*/

/**************************************************************************
    For Windows builds, os.h should always be included first to ensure that
    _WIN32_WINNT is defined before winsock2.h or windows.h get included.
**************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "common.h"


/**
* @enum msgDeleteStrategy
*
* strategies for removing messages from the persistent store of an Endpoint
*/
typedef enum msgDeleteStrategy {
    DEL_NONE     = 0,     /**< don't remove any message. */
    DEL_EVEN     = 1,     /**< remove messages with even message sequence numbers */
    DEL_ODD      = 2,     /**< remove messages with odd message sequence numbers */
    DEL_ALL      = 3      /**< remove all messages */
} msgDeleteStrategy_t;

/**
* @struct rxBrowseFlowCallbackInfo
*
* A structure that is used by the browse flow Rx callback
*/
typedef struct rxBrowseFlowCallbackInfo {
    solClient_uint32_t      msgCount;          /**< Number of received messages. */
    solClient_uint32_t      delCount;          /**< Number of deleted messages. */
    msgDeleteStrategy_t     delStrategy;       /**< strategy for selective removing messages */
} rxBrowseFlowCallbackInfo_t;

/*
* fn rxBrowserFlowMsgCallbackFunc()
* A solClient_flow_createRxCallbackFuncInfo_t that acknowledges
* messages. To be used as part of a solClient_flow_createFuncInfo_t
* passed to a solClient_session_createFlow().
*/
static solClient_rxMsgCallback_returnCode_t
rxBrowserFlowMsgCallbackFunc(solClient_opaqueFlow_pt  opaqueFlow_p,
                             solClient_opaqueMsg_pt   msg_p,
                             void                     *user_p)
{
    solClient_msgId_t               msgId;
    solClient_int64_t               seqNum;
    rxBrowseFlowCallbackInfo_t      *callbackInfo_p;
    solClient_returnCode_t          rc;

    if (user_p == NULL) {
        /* Indicate test suite failure */
        printf("Error: Got null user pointer");
        return SOLCLIENT_CALLBACK_OK;
    }
    callbackInfo_p = (rxBrowseFlowCallbackInfo_t *)user_p;    

    if (opaqueFlow_p == NULL) {
        printf("Error: Got null opaque flow pointer");
        return SOLCLIENT_CALLBACK_OK; /* not taking msg */ 
    }

    if (msg_p == NULL) {
        printf("Error: Got null message pointer");
        return SOLCLIENT_CALLBACK_OK; /* not taking msg */ 
    }

    /* Process the message. */
    rc = solClient_msg_getMsgId(msg_p, &msgId);
    if (rc  != SOLCLIENT_OK) {
        common_handleError(rc, "solClient_msg_getMsgId()");
        return SOLCLIENT_CALLBACK_OK; 
    }

    rc = solClient_msg_getSequenceNumber(msg_p, &seqNum);
    if (rc != SOLCLIENT_OK) {
       common_handleError(rc, "solClient_msg_getSequenceNumber()");
       return SOLCLIENT_CALLBACK_OK; 
    }

    callbackInfo_p->msgCount++;
    printf("Received message on browser flow: MsgID =%lld; SeqNum=%lld).\n", msgId, seqNum);

    if (callbackInfo_p->delStrategy == DEL_ALL) {
        printf("Deleting message from the queue: MsgID =%lld; SeqNum=%lld).\n", msgId, seqNum);
        solClient_flow_sendAck(opaqueFlow_p, msgId);
        callbackInfo_p->delCount++;
    }
    else if ((callbackInfo_p->delStrategy == DEL_ODD) && (seqNum &0x1)) {
        printf("Deleting message from the queue: MsgID =%lld; SeqNum=%lld).\n", msgId, seqNum);
        solClient_flow_sendAck(opaqueFlow_p, msgId);
        callbackInfo_p->delCount++;
    }
    else if ((callbackInfo_p->delStrategy == DEL_EVEN) && !(seqNum &0x1)) {
        printf("Deleting message from the queue: MsgID =%lld; SeqNum=%lld).\n", msgId, seqNum);
        solClient_flow_sendAck(opaqueFlow_p, msgId);
        callbackInfo_p->delCount++;
    }
    return SOLCLIENT_CALLBACK_OK;
}

/*
* fn browserFlow()
* 
*/
static void 
browserFlow(solClient_opaqueSession_pt   session_p,
            char                         *queueName,
            msgDeleteStrategy_t          delStrategy,
            solClient_uint32_t           browseWindow)
{
    static char *delStrategyStr[] = {
        "DEL_NONE",
        "DEL_EVEN",
        "DEL_ODD",
        "DEL_ALL"
    };

    solClient_opaqueFlow_pt             flow_p = NULL;
    const char                         *flowProps[20];
    int                                 propIndex;
    solClient_flow_createFuncInfo_t     flowFuncInfo    =
        SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    rxBrowseFlowCallbackInfo_t          flowCallbackInfo;
    solClient_uint32_t                  sendStartCount = browseWindow;
    int                                 loop;
    char                                browseWindowStr[32];
    solClient_uint32_t                  prevMsgCount;

    if ((delStrategy < DEL_NONE) || (delStrategy > DEL_ALL)) {
      printf("ERROR: Invalid delStrategy value '%d'\n", delStrategy);
      goto cleanup;
    }

    snprintf(browseWindowStr, sizeof(browseWindowStr), "%d", browseWindow);

    /* set the strategy to not removing any message from the queue */
    flowCallbackInfo.delStrategy = delStrategy;
    flowCallbackInfo.msgCount = 0;
    flowCallbackInfo.delCount = 0;

    flowFuncInfo.rxMsgInfo.callback_p = rxBrowserFlowMsgCallbackFunc;
    flowFuncInfo.rxMsgInfo.user_p     = &flowCallbackInfo;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    flowProps[propIndex++] = queueName;

    /*
     * Set 'browser' mode and window size
     */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BROWSER;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_WINDOWSIZE;
    flowProps[propIndex++] = browseWindowStr;

    flowProps[propIndex] = NULL;

    if (solClient_session_createFlow(flowProps,
                                     session_p,
                                     &flow_p,
                                     &flowFuncInfo,
                                     sizeof(flowFuncInfo)) != SOLCLIENT_OK) {
        printf( "Error: solClient_session_createFlow() did not return SOLCLIENT_OK\n"); 
        goto cleanup;
    }

    loop = 0;
    printf("Browsing messages from queue '%s' with a removing message strategy '%s' , "
           "Ctrl-C to stop.....\n", 
           queueName, delStrategyStr[delStrategy]);
    do {
      prevMsgCount = flowCallbackInfo.msgCount;
        if (flowCallbackInfo.msgCount == sendStartCount) {
            solClient_flow_start(flow_p);
            sendStartCount += browseWindow;
        }
        /*
         * Waiting for more messages
         */
        sleepInSec(2);

        if (gotCtlC) {
            goto cleanup;
        }
    } while ( prevMsgCount< flowCallbackInfo.msgCount);

    printf("Number of Received Messages:     %d\n",  flowCallbackInfo.msgCount);
    printf("Number of Deleted Messages:      %d\n",  flowCallbackInfo.delCount);
    
 cleanup: 
    if (flow_p != NULL) {
        if (solClient_flow_destroy(&flow_p) != SOLCLIENT_OK) {
            printf("Error: could not destroy browser flow");
        }
    }
}

/*
* fn main() 
* param appliance ip address
* param appliance username
* 
* The entry point to the application.
*/
int main(int argc, char *argv[])
{
    struct commonOptions                commandOpts;
    solClient_returnCode_t              rc = SOLCLIENT_OK;

    solClient_opaqueContext_pt          context_p;
    solClient_context_createFuncInfo_t  contextFuncInfo = 
        SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    solClient_opaqueSession_pt          session_p;
    char                                queueName[SOLCLIENT_BUFINFO_MAX_QUEUENAME_SIZE];
    solClient_opaqueMsg_pt              msg_p;      /**< The message pointer */
    char                                binMsg[] = COMMON_ATTACHMENT_TEXT;
    solClient_destination_t             destination;
    solClient_destinationType_t         destinationType;
    int                                 publishCount = 0;
    solClient_uint32_t                  browseWindow = 10;

    printf("\nsimpleBrowserFlow.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n");

    /* Intialize Control C handling */
    initSigHandler();

    /*************************************************************************
    * Parse command options
    *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK |
                                PASS_PARAM_MASK |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if (common_parseCommandOptions(argc, argv, &commandOpts, NULL) == 0) {
        exit(1);
    }

    /*************************************************************************
    * Initialize the API and setup logging level
    *************************************************************************/

    /* solClient needs to be initialized before any other API calls. */
    if ((rc = solClient_initialize(SOLCLIENT_LOG_DEFAULT_FILTER,
                                   NULL)) != SOLCLIENT_OK) {
        common_handleError(rc, "solClient_initialize()");
        goto notInitialized;
    }

    common_printCCSMPversion();

    /* 
     * Standard logging levels can be set independently for the API and the
     * application. In this case, the ALL category is used to set the log level for 
     * both at the same time.
     */
    solClient_log_setFilterLevel(SOLCLIENT_LOG_CATEGORY_ALL,
                                 commandOpts.logLevel);

    /*************************************************************************
    * Create a Context
    *************************************************************************/

    solClient_log(SOLCLIENT_LOG_INFO, "Creating solClient context");

    /* 
     * When creating the Context, specify that the Context thread be 
     * created automatically instead of having the application create its own
     * Context thread.
     */    
    if ((rc = solClient_context_create(SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                       &context_p,
                                       &contextFuncInfo,
                                       sizeof(contextFuncInfo))) != SOLCLIENT_OK) {
        common_handleError(rc, "solClient_context_create()");
        goto cleanup;
    }

    /*************************************************************************
    * Create and connect a Session
    *************************************************************************/

    solClient_log(SOLCLIENT_LOG_INFO, "Creating solClient sessions.");

    if ((rc = common_createAndConnectSession(context_p,
                                             &session_p,
                                             common_messageReceivePrintMsgCallback,
                                             common_eventCallback,
                                             NULL,
                                             &commandOpts)) != SOLCLIENT_OK) {
        common_handleError(rc, "common_createAndConnectSession()");
        goto cleanup;
    }

    if (!solClient_session_isCapable(session_p, SOLCLIENT_SESSION_CAPABILITY_BROWSER)) {
      printf("stopping as appliance doesn't have guaranteed delivery\n");
        goto sessionConnected;
    }

    strncpy(queueName, COMMON_TESTQ, sizeof(COMMON_TESTQ));
    destinationType = SOLCLIENT_QUEUE_DESTINATION;

    /*************************************************************************
    * Publish
    *************************************************************************/
    printf("Publishing 30 messages to queue %s, Ctrl-C to stop.....\n", queueName);
    publishCount = 0;
    while (!gotCtlC && publishCount < 30) {

        /*************************************************************************
        * MSG building
        *************************************************************************/
        
        /* Allocate a message. */
        if ((rc = solClient_msg_alloc(&msg_p)) != SOLCLIENT_OK) {
            common_handleError(rc, "solClient_msg_alloc()");
            goto sessionConnected;
        }
        /* Set the delivery mode for the message. */
        if ((rc = solClient_msg_setDeliveryMode(msg_p, 
                 SOLCLIENT_DELIVERY_MODE_PERSISTENT)) != SOLCLIENT_OK) {
            common_handleError(rc, "solClient_msg_setDeliveryMode()");
            goto sessionConnected;
        }
        /* Use a binary attachment and use it as part of the message. */
        if ((rc = solClient_msg_setBinaryAttachment(msg_p, 
                                                       binMsg, 
                                                       sizeof(binMsg))) != SOLCLIENT_OK) {
            common_handleError(rc, "solClient_msg_setBinaryAttachmentPtr()");
            goto sessionConnected;
        }
        

        destination.destType = destinationType;
        destination.dest = queueName;

        if ((rc = solClient_msg_setDestination(msg_p, 
                                               &destination,
                                               sizeof(destination))) != SOLCLIENT_OK) {
            common_handleError(rc, "solClient_msg_setDestination()");
            goto sessionConnected;
        }
        
        if ((rc = solClient_session_sendMsg(session_p, msg_p)) != SOLCLIENT_OK) {
            common_handleError(rc, "solClient_session_send");
            goto sessionConnected;
        }

        if ((rc = solClient_msg_free(&msg_p)) != SOLCLIENT_OK) {
            common_handleError(rc, "solClient_msg_free()");
            goto sessionConnected;
        }
        publishCount ++;
    }

    if (gotCtlC) {
        printf("Got Ctrl-C, cleaning up\n");
        goto sessionConnected;
    }

    /************************************************
     * create a flow to browse the queue without removing any message
     ***************************************************/
    browserFlow(session_p, queueName, DEL_NONE, browseWindow);

    if (gotCtlC) {
        printf("Got Ctrl-C, cleaning up\n");
        goto sessionConnected;
    }

    /************************************************
     * create a flow to browse the queue and remove messages with odd sequence numbers
     ***************************************************/
    browserFlow(session_p, queueName, DEL_ODD, browseWindow);

    if (gotCtlC) {
        printf("Got Ctrl-C, cleaning up\n");
        goto sessionConnected;
    }

    /************************************************
     * create a flow to browse the queue and remove messages with even sequence numbers
     ***************************************************/
    browserFlow(session_p, queueName, DEL_EVEN, browseWindow);

    /************* Cleanup *************/
  sessionConnected:
    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

  cleanup:
    /* Cleanup solClient. */
    if ((rc = solClient_cleanup()) != SOLCLIENT_OK) {
        common_handleError(rc, "solClient_cleanup()");
    }
    
  notInitialized:
    return 0;

}  //End main()
