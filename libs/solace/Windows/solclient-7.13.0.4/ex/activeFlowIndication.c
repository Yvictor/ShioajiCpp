
/** @example ex/activeFlowIndication.c
 */

/*
 * This sample demonstrates:
 *  - Provisioning an exclusive durable Queue on the appliance.
 *  - Binding a first Flow to the provisioned Queue and receiving a
 *    FLOW_ACTIVE event
 *  - Binding a second Flow to the provisioned Queue does not receive an initial
 *    FLOW_ACTIVE event
 *  - Closing the first Flow and receiving a FLOW_ACTIVE event for the second
 *    Flow.
 *  - Cleanup and deprovisioning an exclusive durable Queue on the appliance.
 *
 * Sample Requirements:
 *  - SolOS appliance supporting Queue provisioning and Active Flow Indication.
 *
 * Copyright 2009-2020 Solace Corporation. All rights reserved.
 */


/*****************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *****************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "common.h"

void
flowEventCallbackFunc ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_flow_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
    solClient_errorInfo_pt errorInfo_p;

    switch ( eventInfo_p->flowEvent ) {
        case SOLCLIENT_FLOW_EVENT_UP_NOTICE:
        case SOLCLIENT_FLOW_EVENT_SESSION_DOWN:
        case SOLCLIENT_FLOW_EVENT_ACTIVE:
        case SOLCLIENT_FLOW_EVENT_INACTIVE:

            /* Events are output to STDOUT. */
            printf ( "Received event for %s : %s (%s)\n",
                     (char *)user_p,
                     solClient_flow_eventToString ( eventInfo_p->flowEvent ),
                     eventInfo_p->info_p);
            break;

        case SOLCLIENT_FLOW_EVENT_DOWN_ERROR:
        case SOLCLIENT_FLOW_EVENT_BIND_FAILED_ERROR:
        case SOLCLIENT_FLOW_EVENT_REJECTED_MSG_ERROR:
            /* Extra error information is available on error events */
            errorInfo_p = solClient_getLastErrorInfo (  );

            /* Error events are output to STDOUT. */
            printf ( "flowEventCallbackFunc() called - %s; subCode %s, responseCode %d, reason %s\n",
                     solClient_flow_eventToString ( eventInfo_p->flowEvent ),
                     solClient_subCodeToString ( errorInfo_p->subCode ), errorInfo_p->responseCode, errorInfo_p->errorStr );
            break;

        default:

            /* Unrecognized or deprecated events are output to STDOUT. */
            printf ( "flowEventCallbackFunc() called - %s.  Unrecognized or deprecated event.\n",
                     solClient_flow_eventToString ( eventInfo_p->flowEvent ) );
            break;
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

    /* Command Options. */
    struct commonOptions commandOpts;

    /* Context. */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo =
        SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session. */
    solClient_opaqueSession_pt session_p;

    /* Endpoint provisioning properties. */
    const char     *provProps[20];
    int             provIndex = 0;
    char            provQueueName[80];
    solClient_uint64_t usTime = getTimeInUs (  );
    solClient_bool_t endpointProvisioned = FALSE;
    solClient_errorInfo_pt errorInfo_p;

    /* Flow Properties. */
    int             propIndex = 0;
    const char     *flowProps[20];
    solClient_opaqueFlow_pt flow1_p = NULL;
    solClient_opaqueFlow_pt flow2_p = NULL;
    solClient_flow_createFuncInfo_t flowFuncInfo =
        SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;

    printf ( "\nactiveFlowIndication.c "
             "(Copyright 2009-2020 Solace Corporation. All rights reserved.)\n"
        );

    /* Initialize Control-C handling. */
    initSigHandler (  );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK ),    /* required parameters */
                               (HOST_PARAM_MASK |
                                PASS_PARAM_MASK |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, NULL ) == 0 ) {
        exit (1);
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
     * application. In this case, use the ALL category to set the log level for
     * both at the same time.
     */
    solClient_log_setFilterLevel ( SOLCLIENT_LOG_CATEGORY_ALL,
        commandOpts.logLevel );

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

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient session." );


    /*
     * createAndConnectSession is a common function used in these samples.
     * It is a wrapper for solClient_session_create() that applies some
     * common properties to the Session, some of which are based on the
     * command options. The wrapper also connects the Session.
     */
    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceiveCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Ensure the endpoint provisioning is supported
     *************************************************************************/
    printf ( "Checking for capability SOLCLIENT_SESSION_CAPABILITY_ENDPOINT_MANAGEMENT..." );
    if ( !solClient_session_isCapable ( session_p, SOLCLIENT_SESSION_CAPABILITY_ENDPOINT_MANAGEMENT ) ) {

        solClient_log ( SOLCLIENT_LOG_ERROR, "Endpoint management not supported." );
        goto sessionConnected;
    }
    printf ( "OK\n" );

    /*************************************************************************
     * Ensure Active Flow Indication is supported
     *************************************************************************/
    printf ( "Checking for capability SOLCLIENT_SESSION_CAPABILITY_ACTIVE_FLOW_INDICATION..." );
    if ( !solClient_session_isCapable ( session_p, SOLCLIENT_SESSION_CAPABILITY_ACTIVE_FLOW_INDICATION ) ) {

        solClient_log ( SOLCLIENT_LOG_ERROR, "Active Flow Indication not supported." );
        goto sessionConnected;
    }
    printf ( "OK\n" );

    /*************************************************************************
     * Provision Queue
     *************************************************************************/
    snprintf ( provQueueName, sizeof ( provQueueName ),"sample_ActiveFlowIndication_%llu", usTime % 100000 );

    printf ( "Provisioning queue '%s' ...", provQueueName );

    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUEUE;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
    provProps[provIndex++] = provQueueName;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_ACCESSTYPE;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_ACCESSTYPE_EXCLUSIVE;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_PERMISSION;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PERM_MODIFY_TOPIC;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUOTA_MB;
    provProps[provIndex++] = "100";
    //Enable the Active Flow Indication
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
            goto sessionConnected;
        }
    } else {
        endpointProvisioned = TRUE;
        printf ( "OK\n" );
    }

    /*************************************************************************
     * Bind a Flow to the provisioned endpoint
     *************************************************************************/
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    flowProps[propIndex++] = provQueueName;     /* Queue name */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACTIVE_FLOW_IND;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;
    flowProps[propIndex++] = NULL;

    flowFuncInfo.rxMsgInfo.callback_p = common_flowMessageReceiveAckCallback;
    flowFuncInfo.eventInfo.callback_p = flowEventCallbackFunc;

    printf ( "Creating flow 1..." );
    flowFuncInfo.eventInfo.user_p = "Flow 1";
    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p,
                                               &flow1_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_IN_PROGRESS ) {
        common_handleError ( rc, "solClient_session_createFlow() did not return SOLCLIENT_IN_PROGRESS after session create." );
        goto sessionConnected;
    }
    printf ( "OK.\n" );
    sleepInSec ( 1 );
    printf ( "Creating flow 2..." );
    flowFuncInfo.eventInfo.user_p = "Flow 2";
    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p,
                                               &flow2_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_IN_PROGRESS ) {
        common_handleError ( rc, "solClient_session_createFlow() did not return SOLCLIENT_IN_PROGRESS after session create." );
        goto sessionConnected;
    }
    printf ( "OK.\n" );
    sleepInSec ( 1 );

    printf("Destroying flow 1.\n");
    solClient_flow_destroy( &flow1_p );
    flow1_p = NULL;
    sleepInSec ( 1 );

    /*************************************************************************
     * Cleanup
     *************************************************************************/
 sessionConnected:
    printf ( ".\n" );
    if ( endpointProvisioned ) {
        if (flow1_p)
        {
            printf ( "Destroying flow 1.\n" );
            solClient_flow_destroy ( &flow1_p );
        }
        if (flow2_p)
        {
            printf ( "Destroying flow 2.\n" );
            solClient_flow_destroy ( &flow2_p );
        }
        printf ( "Deprovisioning queue.\n" );
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

notInitialized:
    return 0;
}
