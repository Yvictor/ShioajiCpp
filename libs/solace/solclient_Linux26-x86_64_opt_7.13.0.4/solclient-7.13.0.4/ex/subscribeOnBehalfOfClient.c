
/** @example ex/subscribeOnBehalfOfClient.c
 */

/*
 * This sample shows how to subscribe on behalf of another client. Doing 
 * so requires knowledge of the target client name, as well as possession of
 * the subscription-manager permission.
 * 
 * Two Sessions are connected to the appliance, their ClientNames 
 * are extracted, and Session #1 adds a Topic subscription on 
 * behalf of Session #2. A message is then published on that Topic,
 * which will be received by Session #2.
 *
 * Copyright 2010-2020 Solace Corporation. All rights reserved.
 */

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "common.h"

/*
 * fn main() 
 * param appliance ip address
 * param appliance username
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

    solClient_opaqueSession_pt sessionMgr_p;
    solClient_opaqueSession_pt sessionClient_p;

    const char     *topic_str = "sample/topic/pasta";
    static const char subscriptionManager[] = "Subscription Manager";
    static const char subscriptionClient[] = "Subscription Client";
    char            clientName_a[SOLCLIENT_SESSION_PROP_MAX_CLIENT_NAME_LEN + 1];
    const char     *props[40];
    int             propIndex;


    printf ( "\nsubscribeOnBehalfOfClient.c (Copyright 2010-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control C handling */
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

    /* solClient needs to be initialized before any other API calls are made. */
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
     * When creating the Context, specify that the Context thread is to be 
     * created automatically instead of having the application create its own
     * Context thread.
     */
    if ( ( rc = solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                           &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create and connection a Session
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient sessions." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &sessionMgr_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback,
                                                 ( void * ) subscriptionManager, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &sessionClient_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback,
                                                 ( void * ) subscriptionClient, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    solClient_log ( SOLCLIENT_LOG_INFO, "Checking for capability:  ..." );
    if ( !solClient_session_isCapable ( sessionMgr_p, SOLCLIENT_SESSION_CAPABILITY_SUBSCRIPTION_MANAGER ) ) {
        printf ( "Subscription Manager Not Supported. Exiting" );
        goto sessionConnected;
    } else {
        solClient_log ( SOLCLIENT_LOG_INFO, "OK" );
    }

    /************************************************************************
     * Get ClientName for the second Session
     ************************************************************************/
    if ( solClient_session_getProperty ( sessionClient_p,
                                         SOLCLIENT_SESSION_PROP_CLIENT_NAME,
                                         clientName_a, sizeof ( clientName_a ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_getProperty()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Subscribe through the Session
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Adding subscription %s in %s on behalf of client %s",
                    topic_str, subscriptionManager, subscriptionClient );
    propIndex = 0;
    props[propIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
    props[propIndex++] = SOLCLIENT_ENDPOINT_PROP_CLIENT_NAME;
    props[propIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
    props[propIndex++] = clientName_a;
    props[propIndex++] = NULL;
    if ( ( rc = solClient_session_endpointTopicSubscribe ( props,
                                                           sessionMgr_p,
                                                           SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                           topic_str, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_endpointTopicSubscribe()" );
        goto sessionConnected;
    }

    /************************************************************************
     * Send a message on the manager session and see it received on the client
     * only
     ***********************************************************************/
    if ( common_publishMessage ( sessionMgr_p, ( char * ) topic_str, SOLCLIENT_DELIVERY_MODE_DIRECT ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_publishDirectMessage()" );
        goto sessionConnected;
    }

    /*************************************************************************
    * Wait for half a second
    *************************************************************************/

    printf ( "Sent.\n" );
    sleepInUs ( 500 );
    printf ( "Done.\n" );


    /************* cleanup *************/
  sessionConnected:
    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( sessionMgr_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }
    if ( ( rc = solClient_session_disconnect ( sessionClient_p ) ) != SOLCLIENT_OK ) {
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
