
/** @example ex/perfADSub.c 
 */

/*
 * This sample provides a high throughput Guaranteed Messaging subscribing 
 * example for the Solace Messaging API for C.
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

static int      msgCount_s = 0;
static long long firstMsgRecvTime_s = 0;


/*
 * fn rxPerfCallbackFunc()
 * A solClient_session_rxCallbackFunc_t that counts messages when called.
 * This is to be used as part of a solClient_session_createFuncInfo_t
 * passed to a solClient_session_create().
 */
static          solClient_rxMsgCallback_returnCode_t
rxPerfMsgCallbackFunc ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    /* For the first message, get time of receipt. */
    if ( msgCount_s == 0 ) {
        firstMsgRecvTime_s = getTimeInUs (  );
    }

    msgCount_s++;

    return SOLCLIENT_CALLBACK_OK;
}

/*
 * fn ADS_flowEventCallbackFunc()
 * A solClient_flow_createEventCallbackFuncInfo_t that is empty.
 * This is to be used as part of a solClient_flow_createFuncInfo_t
 * passed to a solClient_session_createFlow().
 */
static void
ADS_flowEventCallbackFunc ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_flow_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
}


/*
 * fn ADS_rxFlowCallbackFunc()
 * A solClient_flow_createRxCallbackFuncInfo_t that counts messages when called.
 * This is to be used as part of a solClient_flow_createFuncInfo_t
 * passed to a solClient_session_createFlow().
 */
static          solClient_rxMsgCallback_returnCode_t
ADS_rxFlowMsgCallbackFunc ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_msgId_t msgId;

    /* For the first message, get time of receipt. */
    if ( msgCount_s == 0 ) {
        firstMsgRecvTime_s = getTimeInUs (  );
    }

    /* Acknowledge the message after processing it. */
    if ( solClient_msg_getMsgId ( msg_p, &msgId )  == SOLCLIENT_OK ) {
        solClient_flow_sendAck ( opaqueFlow_p, msgId );
    }

    msgCount_s++;

    return SOLCLIENT_CALLBACK_OK;
}

/*
 * fn main() 
 * param appliance ip address
 * param appliance username
 * param AD windows size
 * param topic name or queue name to subscribe to
 * param mode one of:
 *       dte: subscribe to a durable Topic Endpoint (the appliance must be equipped with an Assured Delivery Blade (ADB))
 *       queue: subscribe to a Queue (the appliance must be equipped with an ADB)
 *       sub: subscribe to a Topic using the default subscriber flow (the appliance must be
 *       equipped with an ADB)
 * param dte The DTE to bind to in the case of mode==dte
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
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    solClient_opaqueFlow_pt flow_p;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;

    const char     *sessionProps[50];
    int             propIndex;

    const char     *flowProps[20];
    const char     *provProps[20];
    int             provIndex;

    long long       lastMsgRecvTime;
    long long       elapsedTime;
    int             endpointCreated = 0;
    solClient_errorInfo_pt errorInfo_p;

    enum flowMode   subscribeMode;
    char            positionalParms[] = "\tmode         Subscribe Mode (te, queue, sub - default queue).\n";


    /* Set message count to 0. */
    msgCount_s = 0;

    printf ( "\nperfADSub.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n" );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK |
                                DEST_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK |
                                PASS_PARAM_MASK |
                                DURABLE_MASK   |
                                NUM_MSGS_MASK  |
                                WINDOW_SIZE_MASK |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, positionalParms ) == 0 ) {
        exit(1);
    }

    /*
     * If the user specified the deliveryMode string, override the 
     * default 
     */
    if (optind < argc ) {
        if ( strcmp ( "te", argv[optind] ) == 0 ) {
            subscribeMode = TE;
        } else if ( strcmp ( "queue", argv[optind] ) == 0 ) {
            subscribeMode = QUEUE;
        } else if ( strcmp ( "sub", argv[optind] ) == 0 ) {
            subscribeMode = SUBSCRIBER;
        } else {
            printf ( "Invalid mode parameter '%s': must be one of 'te', 'queue', 'sub'\n", argv[optind] );
            exit(1);
        }
    } else {
        subscribeMode = QUEUE;
    }
    initSigHandler (  );

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
     * CREATE A CONTEXT
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

    sessionFuncInfo.rxMsgInfo.callback_p = rxPerfMsgCallbackFunc;
    sessionFuncInfo.eventInfo.callback_p = common_eventPerfCallback;

    propIndex = 0;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = commandOpts.username;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = commandOpts.password;

    if ( commandOpts.targetHost[0] != (char) 0 ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = commandOpts.targetHost;
    }
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL;
    sessionProps[propIndex++] = ( commandOpts.enableCompression ) ? "9" : "0";

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

    sessionProps[propIndex++] = NULL;

    solClient_log ( SOLCLIENT_LOG_INFO, "creating solClient session" );
    if ( ( rc = solClient_session_create ( sessionProps,
                                           context_p, &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) )
         != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto cleanup;
    }

    /* Connect the Session. */
    solClient_log ( SOLCLIENT_LOG_INFO, "connecting solClient session" );
    if ( ( rc = solClient_session_connect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create a Flow (if needed)
     *************************************************************************/

    flowFuncInfo.rxMsgInfo.callback_p = ADS_rxFlowMsgCallbackFunc;
    flowFuncInfo.eventInfo.callback_p = ADS_flowEventCallbackFunc;

    /* Does a Flow need to be created? */
    propIndex = 0;
    provIndex = 0;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;

    /* Set Acknowledge mode to CLIENT_ACK */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;


    if ( commandOpts.gdWindow != 0 ) {
        char            gdWindowStr[32];
        snprintf ( gdWindowStr, sizeof ( gdWindowStr ), "%d", commandOpts.gdWindow );
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_WINDOWSIZE;
        flowProps[propIndex++] = gdWindowStr;
    }

    if ( subscribeMode == TE ) {

        /* Durable Topic Endpoint */
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_TE;

        if ( commandOpts.usingDurable ) {
            flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
            flowProps[propIndex++] = COMMON_TESTDTE;

            /* Durable Endpoint, set provision properties. */
            provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
            provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_TE;
            provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
            provProps[provIndex++] = COMMON_TESTDTE;
        } else {
            flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
            flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
        }

        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_TOPIC;
        flowProps[propIndex++] = commandOpts.destinationName;       /* Topic name */
    } else if ( subscribeMode == QUEUE ) {
        /* Queue */
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUEUE;

        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
        flowProps[propIndex++] = commandOpts.destinationName;       /* Queue name */

        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
        provProps[provIndex++] = commandOpts.destinationName;
    } else if ( subscribeMode == SUBSCRIBER ) {
        /* Guaranteed Message Subscriber */
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_SUB;
    }
    flowProps[propIndex++] = NULL;

    /* Does an endpoint need to be provisioned? Check if provision properties are set. */
    if ( provIndex > 0 ) {

        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_PERMISSION;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PERM_MODIFY_TOPIC;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUOTA_MB;
        provProps[provIndex++] = "100";
        provProps[provIndex++] = NULL;

        /* Try to provision the endpoint. */
        if ( ( rc = solClient_session_endpointProvision ( provProps,
                                                          session_p,
                                                          SOLCLIENT_PROVISION_FLAGS_WAITFORCONFIRM,
                                                          NULL, NULL, 0 ) ) != SOLCLIENT_OK ) {
            errorInfo_p = solClient_getLastErrorInfo (  );
            if ( errorInfo_p != NULL ) {
                if ( ( errorInfo_p->subCode == SOLCLIENT_SUBCODE_ENDPOINT_ALREADY_EXISTS ) ||
                     ( errorInfo_p->subCode == SOLCLIENT_SUBCODE_PERMISSION_NOT_ALLOWED ) ||
                     ( errorInfo_p->subCode == SOLCLIENT_SUBCODE_ENDPOINT_PROPERTY_MISMATCH ) ) {
                    solClient_log ( SOLCLIENT_LOG_INFO,
                                    "solClient_session_endpointProvision() failed subCode (%d:'%s')",
                                    errorInfo_p->subCode, solClient_subCodeToString ( errorInfo_p->subCode ) );
                } else {
                    solClient_log ( SOLCLIENT_LOG_WARNING,
                                    "solClient_session_endpointProvision() failed subCode (%d:'%s')",
                                    errorInfo_p->subCode, solClient_subCodeToString ( errorInfo_p->subCode ) );
                }
            }
        } else {
            endpointCreated = 1;
        }
    }

    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p,
                                               &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_IN_PROGRESS ) {
        common_handleError ( rc, "solClient_session_createFlow() did not return SOLCLIENT_IN_PROGRESS after session create." );
        goto sessionConnected;
    }

    /*************************************************************************
     * Subscribe
     *************************************************************************/

    /* Add the subscription. */
    if ( subscribeMode == SUBSCRIBER ) {
        solClient_log ( SOLCLIENT_LOG_INFO, "adding subscription \"%s\"", commandOpts.destinationName );
        if ( ( rc = solClient_session_topicSubscribe ( session_p, commandOpts.destinationName ) )
             != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_topicSubscribe()" );
            goto sessionConnected;
        }
    }

    /*************************************************************************
     * Wait for messages
     *************************************************************************/

    printf ( "Waiting for messages....." );
    fflush ( stdout );
    while ( ( msgCount_s < commandOpts.numMsgsToSend ) && !gotCtlC ) {
        sleepInUs ( 100 );
    }

    lastMsgRecvTime = getTimeInUs (  );
    elapsedTime = lastMsgRecvTime - firstMsgRecvTime_s;

    printf ( "Recv %d msgs in %lld usec, rate of %Lf msgs/sec\n",
             msgCount_s, elapsedTime, ( long double ) msgCount_s / ( ( long double ) elapsedTime / ( long double ) 1000000.0 ) );


    /*************************************************************************
     * Cleanup
     *************************************************************************/

  sessionConnected:

    /*
     * solClient_cleanup() destroys all objects. This disconnect sessions 
     * with the appliance which implicitly unbinds all Flows. However without an
     * explicit unbind, temporary endpoints linger for 60 seconds. To ensure
     * temporary endpoints are removed immmediately, applications should explicitly
     * destroy Flows and not rely solely on solClient_cleanup().
     */
    if ( flow_p != NULL ) {
        if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_flow_destroy()" );
        }
    }
    if ( endpointCreated ) {
        if ( ( rc = solClient_session_endpointDeprovision ( provProps,
                                                            session_p,
                                                            SOLCLIENT_PROVISION_FLAGS_WAITFORCONFIRM, NULL ) ) != SOLCLIENT_OK ) {
            errorInfo_p = solClient_getLastErrorInfo (  );
            if ( errorInfo_p != NULL ) {
                solClient_log ( SOLCLIENT_LOG_WARNING,
                                "solClient_session_endpointDeprovision() failed subCode (%d:'%s')",
                                errorInfo_p->subCode, solClient_subCodeToString ( errorInfo_p->subCode ) );
            }
        }
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
    goto notInitialized;

  notInitialized:
    /* Nothing to do - just exit. */

    return 0;
}                               //End main()
