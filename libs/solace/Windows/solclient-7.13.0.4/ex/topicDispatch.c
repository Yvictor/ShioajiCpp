
/** @example ex/topicDispatch.c
 */

/*
 * This sample demonstrates using local topicDispatch to direct received messages
 * into specialized received data paths.  
 *
 * This sample performs the following steps:
 *
 * - Adds subscription "a/>" to the appliance to receive all messages with a Topic prefix 
 *   of "a/".
 * - Adds local dispatch function 1 for Topic "a/b"
 * - Add dispatch function 2  and subscription for "c/>"
 * - Add local dispatch function 3 for subscription "c/d"
 * - publish on Topic a/c and verify receipt only on session callback
 * - publish on Topic a/b and verify receipt only on dispatch function 1
 * - publish on Topic c/d and verify receipt on both dispatch functions 2 and 3
 * - publish on Topic c/e and verify receipt on only dispatch function 2
 *
 * Copyright 2007-2020 Solace Corporation. All rights reserved.
 *
 */

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClientMsg.h"
#include "common.h"

struct commonOptions commandOpts;

static int      solMsgRxCount[4];

/*****************************************************************************
 * Received message handling code
 *****************************************************************************/


static          solClient_rxMsgCallback_returnCode_t
solSessionMsgReceiveCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{

    solMsgRxCount[0]++;
    return SOLCLIENT_CALLBACK_OK;
}                               //End solSessionMsgReceiveCallback

static          solClient_rxMsgCallback_returnCode_t
solDispatchMsgReceiveCallback1 ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solMsgRxCount[1]++;
    return SOLCLIENT_CALLBACK_OK;
}                               //End solSessionMsgReceiveCallback

static          solClient_rxMsgCallback_returnCode_t
solDispatchMsgReceiveCallback2 ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solMsgRxCount[2]++;
    return SOLCLIENT_CALLBACK_OK;
}                               //End solSessionMsgReceiveCallback

static          solClient_rxMsgCallback_returnCode_t
solDispatchMsgReceiveCallback3 ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solMsgRxCount[3]++;
    return SOLCLIENT_CALLBACK_OK;
}                               //End solSessionMsgReceiveCallback


/*
 * fn main() 
 * param appliance_ip The message backbone IP address.
 * param appliance_username The client username.
 *
 * The entry point to the application.
 */
int
main ( int argc, char *argv[] )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;
    int             propIndex;

    /*********** Context-related variable definitions*********/
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /*********** Session-related variable definitions*********/
    solClient_opaqueSession_pt session_p;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    const char     *sessionProps[40];

    /************ Dispatch Function definitions *************/
    solClient_session_rxMsgDispatchFuncInfo_t dispatchInfo =
            SOLCLIENT_SESSION_DISPATCHFUNC_INITIALIZER ( SOLCLIENT_DISPATCH_TYPE_CALLBACK );

    /************ Basic initialization *********************/
    printf ( "\ntopicDispatch.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n" );

    /* Initialize the API, this needs to be called before first usage. */
    if ( ( rc = solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_initialize()" );
        goto notInitialized;
    }

    common_printCCSMPversion (  );
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

    sessionFuncInfo.rxMsgInfo.callback_p = solSessionMsgReceiveCallback;
    sessionFuncInfo.rxMsgInfo.user_p = NULL;
    sessionFuncInfo.eventInfo.callback_p = common_eventCallback;
    sessionFuncInfo.eventInfo.user_p = NULL;


    /* Intialize Control C handling. */
    initSigHandler (  );

    /*
     * Standard logging levels can be set independantly for the API and calling
     * applications.
     */
    solClient_log_setFilterLevel ( SOLCLIENT_LOG_CATEGORY_ALL, commandOpts.logLevel );


    /* 
     * Create a Context to use for the Session. In this case, specify that the
     * API automatically create the Context thread instead of having the
     * application create its own context thread.
     */
    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient context" );
    if ( ( rc = solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                           &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto cleanup;
    }

    /* Create a Session for sending/receiving messages. */
    propIndex = 0;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = commandOpts.username;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = commandOpts.password;

    if ( commandOpts.targetHost[0] != (char) 0 ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = commandOpts.targetHost;
    }
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_CONNECT_RETRIES;
    sessionProps[propIndex++] = "3";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES;
    sessionProps[propIndex++] = "3";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL;
    sessionProps[propIndex++] = ( commandOpts.enableCompression ) ? "9" : "0";
    /*
     * Note: Reapplying subscriptions allows Sessions to reconnect after failure and
     * have all their subscriptions automatically restored. For Sessions with many
     * subscriptions this can increase the amount of time required for a successful
     * reconnect.
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    /*
     * Note: Including meta data fields such as sender timestamp, sender ID, and
     * sequence number will reduce the maximum attainable throughput as significant
     * extra encoding/decoding is required. This is true whether the fields are
     * autogenerated or manually added.
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_SEND_TIMESTAMPS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_SENDER_ID;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_SEQUENCE_NUMBER;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    /*************************************************************************
     * Enable Topic Dispatch on the Session.
     *************************************************************************/
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_TOPIC_DISPATCH;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    if ( commandOpts.vpn[0] ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
        sessionProps[propIndex++] = commandOpts.vpn;
    }
    /*
     * The certificate validation property is ignored on non-SSL sessions.
     * For simple demo applications, disable it on SSL sesssions (host
     * string begins with tcps:) so a local trusted root and certificate
     * store is not required. See the  API usres guide for documentation
     * on how to setup a trusted root so the servers certificate returned
     * on the secure connection can be verified if this is desired.
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_VALIDATE_CERTIFICATE;
    sessionProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
    if ( commandOpts.useGSS ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME;
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME_GSS_KRB;
    }

    sessionProps[propIndex] = NULL;

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient session" );
    if ( ( rc = solClient_session_create ( sessionProps,
                                           context_p,
                                           &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto cleanup;
    }

    /* Connect the Session. */
    solClient_log ( SOLCLIENT_LOG_INFO, "Connecting solClient session" );
    if ( ( rc = solClient_session_connect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
        goto cleanup;
    }

    /***************Subscription initialization*******************************/

    /*************************************************************************
     * Add Session subscription
     *************************************************************************/

    if ( ( rc = solClient_session_topicSubscribeExt ( session_p,
                                                      SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM, "a/>" ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Add first dispatch function. This dispatch is local only (does not add
     * the subscription on the appliance).
     *************************************************************************/
    dispatchInfo.callback_p = solDispatchMsgReceiveCallback1;
    dispatchInfo.user_p = NULL;                 /* don't care for purpose of this sample */
    if ( ( rc = solClient_session_topicSubscribeWithDispatch ( session_p,
                                                               ( SOLCLIENT_SUBSCRIBE_FLAGS_LOCAL_DISPATCH_ONLY ),
                                                               "a/b", &dispatchInfo, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribeWithDispatch()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Add second dispatch function. This dispatch includes adding the 
     * subscription to the appliance.
     *************************************************************************/
    dispatchInfo.callback_p = solDispatchMsgReceiveCallback2;
    dispatchInfo.user_p = NULL; /* don't care for purpose of this sample */
    if ( ( rc = solClient_session_topicSubscribeWithDispatch ( session_p,
                                                               SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                               "c/>", &dispatchInfo, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribeWithDispatch()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Add third dispatch function. This dispatch is local only (no subscription 
     * is added to appliance).
     *************************************************************************/
    dispatchInfo.callback_p = solDispatchMsgReceiveCallback3;
    dispatchInfo.user_p = NULL;                 /* don't care for purpose of this sample */
    if ( ( rc = solClient_session_topicSubscribeWithDispatch ( session_p,
                                                               ( SOLCLIENT_SUBSCRIBE_FLAGS_LOCAL_DISPATCH_ONLY ),
                                                               "c/d", &dispatchInfo, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribeWithDispatch()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Publish a message on each of the four Topics
     *************************************************************************/

    printf ( "Publishing messages\n\n" );
    common_publishMessage ( session_p, "a/c", SOLCLIENT_DELIVERY_MODE_DIRECT );
    common_publishMessage ( session_p, "a/b", SOLCLIENT_DELIVERY_MODE_DIRECT );
    common_publishMessage ( session_p, "c/d", SOLCLIENT_DELIVERY_MODE_DIRECT );
    common_publishMessage ( session_p, "c/e", SOLCLIENT_DELIVERY_MODE_DIRECT );


    /*************************************************************************
     * Pause to let messages return
     *************************************************************************/

    sleepInSec ( 1 );

    /*************************************************************************
     * Verify expected results
     *************************************************************************/

    if ( solMsgRxCount[0] == 1 ) {
        printf ( "Received exactly one message on session callback as expected\n" );
    } else {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Received %d message(s) on session callback - 1 expected", solMsgRxCount[0] );
    }
    if ( solMsgRxCount[1] == 1 ) {
        printf ( "Received exactly one message on dispatch callback for topic 'a/b' as expected\n" );
    } else {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Received %d message(s) on dispatch callback for topic 'a/b' - 1 expected",
                        solMsgRxCount[1] );
    }
    if ( solMsgRxCount[2] == 2 ) {
        printf ( "Received exactly two messages on dispatch callback for topic 'c/>' as expected\n" );
    } else {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Received %d message(s) on dispatch callback for topic 'c/>' - 2 expected",
                        solMsgRxCount[2] );
    }
    if ( solMsgRxCount[3] == 1 ) {
        printf ( "Received exactly one message on dispatch callback for topic 'c/d' as expected\n" );
    } else {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Received %d message(s) on dispatch callback for topic 'c/d' - 1 expected",
                        solMsgRxCount[3] );
    }

    /************* Cleanup *************/
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
