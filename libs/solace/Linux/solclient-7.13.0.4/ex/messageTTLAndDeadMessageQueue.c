
/** @example ex/messageTTLAndDeadMessageQueue.c 
 */

/*
 * This sample demonstrates:
 * - Setting TimeToLive on a published message.
 * - Setting Dead Message Queue Eligible on a published message.
 * - Publishing messages with and without a Time-to-Live (TTL) and verifying
 *   expected results.
 *
 * Sample Requirements:
 *  - A Solace running SolOS-TR
 *
 *
 * In this sample, a Session to a SolOS-TR appliance is created and then the
 * following tasks are performed:
 * - create a durable Queue endpoint
 * - create a Dead Message Queue (DMQ)
 * - publish three messages with TTL=0
 * - publish one message with TTL=3000, DMQ=FALSE
 * - publish one message with TTL=3000, DMQ=TRUE
 * - bind to the Queue and verify all that five messages were received 
 * - flow control queue (solClient_flow_stop)
 * - publish five messages again
 * - wait five seconds
 * - open a transport window to Queue (solClient_flow_start)
 * - verify only three messages are received
 * - bind to the Dead Message Queue
 * - verify that one message was received
 * - remove the durable Queue and DMQ
 *
 * Copyright 2010-2020 Solace Corporation. All rights reserved.
 */


/*****************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *****************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"


/*****************************************************************************
 * publishMessageWithTTL
 *
 * Publishes an empty message to the Topic MY_SAMPLE_TOPIC. An empty message is
 * used because we only care about whether it gets delivered and not
 * what the contents are. Set TTL and DMQE in the message.
 *
 *****************************************************************************/
static void
publishMessageWithTTL ( solClient_opaqueSession_pt session_p, solClient_int64_t ttl, solClient_bool_t dmqe )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;

    solClient_log ( SOLCLIENT_LOG_DEBUG, "About to publish\n" );

    /* Allocate memory for the message to be sent. */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        return;
    }

    /* Set the message delivery mode. */
    if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_NONPERSISTENT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto freeMessage;
    }

    /* Set the destination. */
    destination.destType = SOLCLIENT_TOPIC_DESTINATION;
    destination.dest = COMMON_MY_SAMPLE_TOPIC;
    if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMessage;
    }

    if ( ( rc = solClient_msg_setTimeToLive ( msg_p, ttl ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setTimeToLive()" );
        goto freeMessage;
    }

    if ( ( rc = solClient_msg_setDMQEligible ( msg_p, dmqe ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDMQEligible()" );
        goto freeMessage;
    }

    /* Send the message. */
    if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_sendMsg()" );
        goto freeMessage;
    }

  freeMessage:
    if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
    }
}


/*****************************************************************************
 * main
 * 
 * The entry point to the application.
 *****************************************************************************/
int
main ( int argc, char *argv[] )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;

    /* Command Options */
    struct commonOptions commandOpts;

    /* Context */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session */
    solClient_opaqueSession_pt session_p;
    /* 
     * User pointers can be used when creating a Session to allow applications
     * to have access to additional information from within its callback
     * functions. The user pointer can refer to anything (void*), but in this
     * sample, a simple char* is used to give the Session a name that
     * is used by the message receive callback to indicate which
     * Session actually received the message.
     *
     * Note: It is important that the data pointed to by the user pointer
     * remains valid for the duration of the Context. With the simplicity of
     * this sample, the stack can be used, but using the heap may be
     * required for more complex applications.
     */
    char           *user_p = "Session Callback";

    /* Flow   */
    solClient_opaqueFlow_pt flow_p = NULL;
    solClient_opaqueFlow_pt dmqFlow_p = NULL;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    /*
     * Pass address of flowRxMsgCounter and dmqRxMsgCounter as user pointer
     * in flowFuncInfo.
     * NOTE: It is important that the data pointed to by the user pointer
     * is valid in the Context of the callback. With the simplicity of
     * this sample, the stack can be used, but use of the heap might be
     * required for more complex applications.
     */
    int    flowRxMsgCounter;
    int    dmqRxMsgCounter;
    /* Props - Properties used to create various objects */
    const char     *props[40];
    int             propIndex;

    int             i;


    printf ( "\nmessageTTLAndDeadMessageQueue.c (Copyright 2010-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control-C handling. */
    initSigHandler (  );

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
     * client application. In this case, the ALL category is used to set the
     * log level for both at the same time.
     */
    solClient_log_setFilterLevel ( SOLCLIENT_LOG_CATEGORY_ALL, commandOpts.logLevel );

    /*************************************************************************
     * Create a Context
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient context" );

    /* 
     * While creating the Context, specify that the Context thread be 
     * created automatically instead of having the application create its own
     * Context thread.
     */
    if ( ( rc = solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                           &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create a Session
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient sessions." );

    /* 
     * createAndConnectSession is a common function used in these samples.
     * It is a wrapper for solClient_session_create() that applies some 
     * common properties to the Session, some of which are based on the 
     * command options. The wrapper also connects the Session.
     */
    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceiveCallback,
                                                 common_eventCallback, user_p, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Ensure that TTL is supported on the appliance
     *************************************************************************/

    /* The same appliance is used for all Sessions, just check one. */
    if ( !solClient_session_isCapable ( session_p, SOLCLIENT_SESSION_CAPABILITY_ENDPOINT_MESSAGE_TTL ) ) {

        solClient_log ( SOLCLIENT_LOG_ERROR, "Time to live is not supported by appliance." );
        goto sessionConnected;
    }

    /************************************************************************
     * Provision a Queue on the appliance 
     ***********************************************************************/
    solClient_log ( SOLCLIENT_LOG_INFO, "Creating Queue %s on appliance.", COMMON_TESTQ );
    if ( ( rc = common_createQueue ( session_p, COMMON_TESTQ ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createQueue()" );
        goto sessionConnected;
    }

    /************************************************************************
     * Provision a Dead Message Queue on the appliance 
     ***********************************************************************/
    solClient_log ( SOLCLIENT_LOG_INFO, "Creating Queue %s on appliance.", COMMON_DMQ_NAME );
    if ( ( rc = common_createQueue ( session_p, COMMON_DMQ_NAME ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createQueue()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Subscribe through the Session
     *************************************************************************/
    solClient_log ( SOLCLIENT_LOG_INFO,
                    "Adding subscription %s to queue %s through Session.", COMMON_MY_SAMPLE_TOPIC, COMMON_TESTQ );

    propIndex = 0;
    props[propIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
    props[propIndex++] = SOLCLIENT_ENDPOINT_PROP_QUEUE;
    props[propIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
    props[propIndex++] = COMMON_TESTQ;
    props[propIndex++] = NULL;
    if ( ( rc = solClient_session_endpointTopicSubscribe ( props,
                                                           session_p,
                                                           SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                           COMMON_MY_SAMPLE_TOPIC, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_endpointTopicSubscribe()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Publish three messages without a TTL or DMQ
     *************************************************************************/

    printf ( "Publishing three messages without TTL and DMQ\n" );

    for ( i = 0; i < 3; i++ ) {
        publishMessageWithTTL ( session_p, 0, FALSE );
    }

    /*************************************************************************
     * Publish a message with TTL=3000 and DMQ=FALSE
     ************************************************************************/

    printf ( "Publshing message with TTL=3000 ms and DMQ Eligible=FALSE\n" );
    publishMessageWithTTL ( session_p, 3000, FALSE );

    /*************************************************************************
     * Publish a message with TTL=3000 and DMQ=TRUE
     ************************************************************************/

    printf ( "Publshing message with TTL=3000 ms and DMQ Eligible=TRUE\n" );
    publishMessageWithTTL ( session_p, 3000, TRUE );

    /*************************************************************************
     * Create a Flow to the Queue
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Bind to queue %s.", COMMON_TESTQ );
    flowRxMsgCounter = 0;
    flowFuncInfo.rxMsgInfo.callback_p = common_flowMessageReceiveCallback;
    flowFuncInfo.rxMsgInfo.user_p = (void *)(&flowRxMsgCounter);
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;

    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    props[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    props[propIndex++] = COMMON_TESTQ;
    props[propIndex] = NULL;

    if ( ( rc = solClient_session_createFlow ( props,
                                               session_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_createFlow() did not return SOLCLIENT_OK" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Create a Flow to the Dead Message Queue
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Bind to queue %s.", COMMON_DMQ_NAME );
    dmqRxMsgCounter = 0;
    flowFuncInfo.rxMsgInfo.callback_p = common_flowMessageReceiveCallback;
    flowFuncInfo.rxMsgInfo.user_p = (void *)(&dmqRxMsgCounter);
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;

    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    props[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    props[propIndex++] = COMMON_DMQ_NAME;
    props[propIndex] = NULL;

    if ( ( rc = solClient_session_createFlow ( props,
                                               session_p,
                                               &dmqFlow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_createFlow() did not return SOLCLIENT_OK" );
        goto sessionConnected;
    }

    /* Wait for up to 250 milliseconds for the messages to be received. */
    i = 0;
    while ( ( flowRxMsgCounter != 5 ) && ( i < 8 ) ) {
        sleepInUs ( 250 );
        i++;
    }
    if ( flowRxMsgCounter != 5 ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "%d messages received on flow, 5 messages expected", flowRxMsgCounter );
        goto sessionConnected;
    }
    if ( dmqRxMsgCounter != 0 ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "%d messages received on DMQ, no messages expected", dmqRxMsgCounter );
        goto sessionConnected;
    }

    printf ( "All sent messages received\n" );

    /*************************************************************************
     * Stop the Flow for TTL and DMQ tests
     *************************************************************************/

    if ( ( rc = solClient_flow_stop ( flow_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_stop()" );
        goto sessionConnected;
    }

    flowRxMsgCounter = 0;

    /*************************************************************************
     * Publish three messages without TTL and DMQ
     *************************************************************************/

    printf ( "Resend 5 messages\n" );

    for ( i = 0; i < 3; i++ ) {
        publishMessageWithTTL ( session_p, 0, FALSE );
    }

    /*************************************************************************
     * Publish a message with TTL=3000 and DMQ=FALSE
     ************************************************************************/
    publishMessageWithTTL ( session_p, 3000, FALSE );

    /*************************************************************************
     * Publish a message with TTL=3000 and DMQ=TRUE
     ************************************************************************/
    publishMessageWithTTL ( session_p, 3000, TRUE );

    printf ( "Wait five seconds to allow messages to expire\n" );
    /* Wait for messages to expire */
    sleepInSec ( 5 );

    /* Should have received one message on DMQ */
    if ( dmqRxMsgCounter != 1 ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "%d messages received on DMQ, 1 messages expected", dmqRxMsgCounter );
        goto sessionConnected;
    }

    /* Restart the Flow to Queue and look for three messages that did not have TTLs. */
    if ( ( rc = solClient_flow_start ( flow_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_start()" );
        goto sessionConnected;
    }

    /* Wait up to a few seconds for messages to be received. */
    i = 0;
    while ( ( flowRxMsgCounter != 3 ) && ( i < 8 ) ) {
        sleepInUs ( 250 );
        i++;
    }
    if ( flowRxMsgCounter != 3 ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "%d messages received on flow, 3 messages expected", flowRxMsgCounter );
        goto sessionConnected;
    }
    printf ( "Three messages with no TTL received and one message received on Dead Message Queue as expected\n" );




    /*************************************************************************
     * CLEANUP
     *************************************************************************/
  sessionConnected:
    if ( flow_p != NULL ) {
        /*
         * Destroy the Flow before deleting the Queue or else the API will log at 
         * NOTICE level for the unsolicited unbind.
         */
        if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_flow_destroy()" );
        }
    }

    if ( dmqFlow_p != NULL ) {
        /*
         * Destroy the Flow before deleting the Queue or else the API will log at NOTICE
         * level for the unsolicited unbind.
         */
        if ( ( rc = solClient_flow_destroy ( &dmqFlow_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_flow_destroy()" );
        }
    }


    /************************************************************************
     * Remove Queues from the appliance
     ***********************************************************************/
    if ( ( rc = common_deleteQueue ( session_p, COMMON_TESTQ ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_deleteQueue()" );
    }

    if ( ( rc = common_deleteQueue ( session_p, COMMON_DMQ_NAME ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_deleteQueue()" );
    }

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

}
