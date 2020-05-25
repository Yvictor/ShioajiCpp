
/** @example ex/cutThroughFlowToQueue.c
 */

/*
 * This sample shows how to create a cut-through message Flow to a queue.
 * It demonstrates:
 *    - Binding to a Queue (temporary or durable)
 *    - Client acknowledgement.
 *
 * For the case of a durable queue, this sample requires that a durable Queue
 * called 'my_sample_queue' be provisioned on the appliance with at least 'Consume' 
 * permissions. 
 * 
 * Cut-through persistence is not a low latency replacement for traditional
 * Solace guaranteed messaging. It is intended only for guaranteed messaging
 * applications that require the absolute lowest possible latency and can trade
 * off feature support and interoperability to achieve it. It is further intended
 * only for applications with relatively low message rates (less than 100k msgs/s)
 * and a small number of clients (hundreds).
 *
 * See the Cut-Through Persistence Technical Note for details on implementing and
 * managing applications that use cut-through persistence.
 *
 * Copyright 2007-2020 Solace Corporation. All rights reserved.
 */

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *************************************************************************/
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

    const char     *flowProps[20];
    int             propIndex;
    char            queueName[SOLCLIENT_BUFINFO_MAX_QUEUENAME_SIZE];

    solClient_opaqueMsg_pt msg_p;                   /**< The message pointer */
    char            binMsg[] = COMMON_ATTACHMENT_TEXT;
    solClient_destination_t destination;
    solClient_destinationType_t destinationType;
    int             publishCount = 0;

    printf ( "\ncutThroughFlowToQueue.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control C handling */
    initSigHandler (  );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK | 
                                PASS_PARAM_MASK |
                                DURABLE_MASK  |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
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
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

    if ( commandOpts.usingDurable ) {
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
        flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

        destinationType = SOLCLIENT_QUEUE_DESTINATION;
        strncpy(queueName, COMMON_TESTQ, sizeof(COMMON_TESTQ));
    } else {
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
        flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
        /*
         * Generate a unique queue name portion. This is not necessary because if the
         * Queue name is left blank a unique name is generated by the API. However,
         * this demonstrates the use of solClient_generateUUIDString().
         */
        if ( ( rc = solClient_generateUUIDString ( queueName, SOLCLIENT_BUFINFO_MAX_QUEUENAME_SIZE ) )
             != SOLCLIENT_OK ) {
            solClient_log ( SOLCLIENT_LOG_INFO,
                            "solClient_generateUUIDString() did not return SOLCLIENT_OK " "after session create. rc = %d ", rc );
            goto sessionConnected;
        }
        destinationType = SOLCLIENT_QUEUE_TEMP_DESTINATION;
    }
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

    /*
     * Cut-through persistence is not a low latency replacement for traditional
     * Solace guaranteed messaging. It is intended only for guaranteed messaging
     * applications that require the absolute lowest possible latency and can trade
     * off feature support and interoperability to achieve it. It is further intended
     * only for applications with relatively low message rates (less than 100k msgs/s)
     * and a small number of clients (hundreds).
     *
     * See the Cut-Through Persistence Technical Note for details on implementing and
     * managing applications that use cut-through persistence.
     */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_FORWARDING_MODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_FORWARDING_MODE_CUT_THROUGH;

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

        if ( commandOpts.usingDurable ) {
            destination.destType = destinationType;
            destination.dest = queueName;
        } else {

            /*
             * Retrieve the temporary queue name from the Flow.  
             * NOTE: solClient_flow_getDestination()
             * can be used on temporary Queues or durable Flows. This sample demonstrates both.
             */
            if ( ( rc = solClient_flow_getDestination ( flow_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
                common_handleError ( rc, "solClient_flow_getDestination()" );
                goto sessionConnected;
            }
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

    /*************************************************************************
     * Wait for CTRL-C
     *************************************************************************/

    if ( gotCtlC ) {
        printf ( "Got Ctrl-C, cleaning up\n" );
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
