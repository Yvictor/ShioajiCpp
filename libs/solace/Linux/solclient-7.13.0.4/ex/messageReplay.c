
/** @example ex/simpleFlowToQueue.c
 */

/*
 * This sample shows how to create message Flows to Queues and replay the messages after they are consumered.
 * It demonstrates:
 *    - Binding to a Queue with a replay log 
 *    - populating and consuming messages from a queue
 *    - replaying consumed messages from a queue
 *    - Client acknowledgement.
 *
 * For message replay of a durabled queue, this sample requires that message vpn the
 * session is connected to has a provisioned replay log. To get replayed messages.
 *
 * Copyright 2007-2020 Solace Corporation. All rights reserved.
 */

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "common.h"

/*
 * fn flowMsgCallbackFunc()
 * A solClient_flow_createRxCallbackFuncInfo_t that acknowledges
 * messages. To be used as part of a solClient_flow_createFuncInfo_t
 * passed to a solClient_session_createFlow().
 */
static          solClient_rxMsgCallback_returnCode_t
flowMsgCallbackFunc ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_returnCode_t rc;
    solClient_msgId_t msgId;

    /* Process the message. */
    if ( solClient_msg_getMsgId ( msg_p, &msgId ) == SOLCLIENT_OK ) {
        printf ( "Received message on flow. (Message ID: %lld).\n", msgId );
    } else {
        printf ( "Received message on flow.\n" );
    }
    if ( ( rc = solClient_msg_dump ( msg_p, NULL, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_dump()" );
        return SOLCLIENT_CALLBACK_OK;
    }

    /* Acknowledge the message after processing it. */
    printf ( "Acknowledging message: %lld.\n", msgId );
    if ( ( rc = solClient_msg_getMsgId ( msg_p, &msgId ) ) == SOLCLIENT_OK ) {
        if ( ( rc = solClient_flow_sendAck ( opaqueFlow_p, msgId ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_flow_sendAck()" );
        }
    } else {
        common_handleError ( rc, "solClient_msg_getMsgId()" );
    }
    (*((int*)user_p))++; /* increment received message counter */
    return SOLCLIENT_CALLBACK_OK;
}

/*
 * fn main() 
 * param appliance ip address
 * param appliance username
 * param durability of the queue 
 * 
 * The entry point to the application.
 */
int
main ( int argc, char *argv[] )
{
    struct commonOptions commandOpts;
    solClient_returnCode_t rc = SOLCLIENT_OK;

    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    solClient_opaqueSession_pt session_p;

    solClient_opaqueFlow_pt flow_p;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    const char     *flowProps[100];
    int             propIndex;
    char            queueName[SOLCLIENT_BUFINFO_MAX_QUEUENAME_SIZE];

    solClient_opaqueMsg_pt msg_p;                   /**< The message pointer */
    char            binMsg[] = COMMON_ATTACHMENT_TEXT;
    solClient_destination_t destination;
    solClient_destinationType_t destinationType;
    int             publishCount = 0;
    int             receivedCount= 0;
    UINT64          publishStartTime = 0;
    char            replayStartLocationStr[250];

    printf ( "\nmessageReplay.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control C handling */
    initSigHandler (  );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK |
                                 DEST_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK |
                                PASS_PARAM_MASK |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                 REPLAY_START_MASK |
                                ZIP_LEVEL_MASK ) );                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, NULL ) == 0 ) {
        exit(1);
    }

    /*************************************************************************
     * Initialize the API and setup logging level
     *************************************************************************/

    /* solClient needs to be initialized before any other API calls. */
    if ( ( rc = solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_initialize()" );
        goto notInitialized;
    }

    common_printCCSMPversion (  );

    /* 
     * Standard logging levels can be set independently for the API and the
     * application. In this case, the ALL category is used to set the log level for 
     * both at the same time.
     */
    solClient_log_setFilterLevel ( SOLCLIENT_LOG_CATEGORY_ALL, commandOpts.logLevel );

    /*************************************************************************
     * Create a Context
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient context" );

    /* 
     * When creating the Context, specify that the Context thread be 
     * created automatically instead of having the application create its own
     * Context thread.
     */
    if ( ( rc = solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                           &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create and connect a Session
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient sessions." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create a Flow
     *************************************************************************/

    flowFuncInfo.rxMsgInfo.callback_p = flowMsgCallbackFunc;
    flowFuncInfo.rxMsgInfo.user_p     = &receivedCount;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    strncpy(queueName, commandOpts.destinationName, sizeof(commandOpts.destinationName));
    destinationType = SOLCLIENT_QUEUE_DESTINATION;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    flowProps[propIndex++] = queueName;

    /* 
     * Client Acknowledgement, which means that the received messages on the Flow 
     * must be explicitly acked, otherwise they are be redelivered to the client
     * when the Flow reconnects.
     * The Client Acknowledgement was chosen to show this particular acknowledgement
     * mode and that clients can use Auto Acknowledgement instead.
     */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;

    flowProps[propIndex] = NULL;

    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_INFO,
                        "solClient_session_createFlow() did not return SOLCLIENT_OK " "after session create. rc = %d ", rc );
        goto sessionConnected;
    }


    /*************************************************************************
     * Publish
     *************************************************************************/
    printf ( "Publishing 10 messages to queue %s, Ctrl-C to stop.....\n", queueName );
    publishCount = 0;
    destination.destType = destinationType;
    destination.dest = queueName;
    publishStartTime = getTimeInUs() / 1000000; /* get current time in seconds */
    /* set replay start time */
    if ( commandOpts.replayStartLocation[0] == 0 ) {
        /* use publish message start time using integer long representation */
        snprintf(replayStartLocationStr, sizeof(replayStartLocationStr), "Date:%lld", publishStartTime);
    } else {
        /* use value passed in from command options for replay start location */
        snprintf(replayStartLocationStr, sizeof(replayStartLocationStr), "%s", commandOpts.replayStartLocation );
    }
    sleepInSec ( 1 );
    while ( !gotCtlC && publishCount < 10 ) {

        /*************************************************************************
         * MSG building
         *************************************************************************/

        /* Allocate a message. */
        if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_alloc()" );
            goto cleanupFlow;
        }
        /* Set the delivery mode for the message. */
        if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
            goto cleanupFlow;
        }
        /* Use a binary attachment and use it as part of the message. */
        if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p, binMsg, sizeof ( binMsg ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setBinaryAttachmentPtr()" );
            goto cleanupFlow;
        }

        if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDestination()" );
            goto cleanupFlow;
        }

        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_send" );
            goto cleanupFlow;
        }

        if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_free()" );
            goto cleanupFlow;
        }
        publishCount++;

        sleepInSec ( 1 );
    }

    if ( gotCtlC ) {
        printf ( "Got Ctrl-C, cleaning up\n" );
        goto cleanupFlow;
    }

    /* cleanup consuming flow */
    if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_destroy()" );
    }

    /* add flow replay start properties */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_REPLAY_START_LOCATION;
    flowProps[propIndex++] = replayStartLocationStr;
    flowProps[propIndex] = NULL;

    printf("Start Flow Message Replay with replay start location '%s'\n", replayStartLocationStr);
    /* re-create flow with replay start location */
    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_INFO,
                        "solClient_session_createFlow() did not return SOLCLIENT_OK " "for flow replay. rc = %d ", rc );
        goto sessionConnected;
    }
    while ( !gotCtlC &&  receivedCount < 2 * publishCount ) {
        sleepInSec ( 1 );
    }

    /*************************************************************************
     * Wait for CTRL-C
     *************************************************************************/

    if ( gotCtlC ) {
        printf ( "Got Ctrl-C, cleaning up\n" );
        goto cleanupFlow;
    }


    /************* Cleanup *************/

  cleanupFlow:
    if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_destroy()" );
    }

  sessionConnected:
    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

  cleanup:
    /* Cleanup solClient. */
    if ( ( rc = solClient_cleanup (  ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cleanup()" );
    }

  notInitialized:
    return 0;

}                               //End main()
