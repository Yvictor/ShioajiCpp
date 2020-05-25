
/** @example ex/queueProvision.c 
 */

/*
 * This sample demonstrates:
 *  - Provisioning a durable Queue on the appliance.
 *  - Binding a Flow to the provisioned Queue and receiving messages from it.
 *  - Publishing messages to the provisioned Queue.
 *
 * Sample Requirements:
 *  - SolOS appliance supporting Queue provisioning.
 *
 * Copyright 2009-2020 Solace Corporation. All rights reserved.
 */


/*****************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *****************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
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
    solClient_msgId_t msgId;

    /* Process the message. */
    if ( solClient_msg_getMsgId ( msg_p, &msgId ) == SOLCLIENT_OK ) {
        printf ( "Received message on flow. (Message ID: %llu).\n", msgId );
        solClient_flow_sendAck ( opaqueFlow_p, msgId );
    } else {
        printf ( "Received message on flow.\n" );
    }
    return SOLCLIENT_CALLBACK_OK;
}


/*
 * Publish a message to the Queue.
 */
solClient_returnCode_t
sendQueueMessage ( solClient_destination_t destination, solClient_opaqueSession_pt session_p )
{
    solClient_returnCode_t rc;
    solClient_opaqueMsg_pt msg_p;
    char            binMsg[1024];

    /* Allocate a message. */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        return SOLCLIENT_FAIL;
    }
    /* Set the delivery mode for the message. */
    if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        return SOLCLIENT_FAIL;
    }
    /* Initialize a binary attachment and use it as part of the message. */
    memset ( ( void * ) binMsg, 0xab, sizeof ( binMsg ) );
    if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p, binMsg, sizeof ( binMsg ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setBinaryAttachmentPtr()" );
        return SOLCLIENT_FAIL;
    }

    if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        return SOLCLIENT_FAIL;
    }

    if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_send" );
        return SOLCLIENT_FAIL;
    }

    if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
        return SOLCLIENT_FAIL;
    }

    return SOLCLIENT_OK;
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
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

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
    solClient_opaqueFlow_pt flow_p;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;

    solClient_destination_t destination;

    printf ( "\nqueueProvision.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control-C handling. */
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

    if (commandOpts.usingDurable) { 

        /*************************************************************************
         * Provision Durable Queue
         *************************************************************************/
        snprintf ( provQueueName, sizeof ( provQueueName ), "sample_queue_Provision_%llu", usTime % 100000 );
        printf ( "Provisioning durable queue '%s' ...", provQueueName );

        provIndex = 0;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUEUE;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
        provProps[provIndex++] = provQueueName;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_PERMISSION;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PERM_MODIFY_TOPIC;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUOTA_MB;
        provProps[provIndex++] = "100";
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_MAXMSG_SIZE;
        provProps[provIndex++] = "500000";
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_MAXMSG_REDELIVERY;
        provProps[provIndex++] = "15";
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_DISCARD_BEHAVIOR;
        provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_DISCARD_NOTIFY_SENDER_ON;
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
    }

    /*************************************************************************
     * Create and bind a Flow to the provisioned durable endpoint or a temporary queue
     *************************************************************************/
    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
    if (commandOpts.usingDurable) {
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
        flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
     }
    else {

        /*
         * Generate a unique queue name portion. This is not necessary because if the
         * Queue name is left blank a unique name is generated by the API. However,
         * this demonstrates the use of solClient_generateUUIDString().
         */
        if ( ( rc = solClient_generateUUIDString ( provQueueName, SOLCLIENT_BUFINFO_MAX_QUEUENAME_SIZE ) )
             != SOLCLIENT_OK ) {
            solClient_log ( SOLCLIENT_LOG_INFO,
                       "solClient_generateUUIDString() did not return SOLCLIENT_OK " "after session create. rc = %d ", rc );
            goto sessionConnected;
        }
        /*************************************************************************
         * Provision Temporary Queue
         *************************************************************************/
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
        flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
        flowProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_PERMISSION;
        flowProps[propIndex++] = SOLCLIENT_ENDPOINT_PERM_MODIFY_TOPIC;
        flowProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_QUOTA_MB;
        flowProps[propIndex++] = "100";
        flowProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_MAXMSG_SIZE;
        flowProps[propIndex++] = "500000";
        flowProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_MAXMSG_REDELIVERY;
        flowProps[propIndex++] = "15";
        flowProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_DISCARD_BEHAVIOR;
        flowProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_DISCARD_NOTIFY_SENDER_ON;
    }

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    flowProps[propIndex++] = provQueueName;     /* Queue name */
    /* Set Acknowledge mode to CLIENT_ACK */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;
    flowProps[propIndex++] = NULL;

    flowFuncInfo.rxMsgInfo.callback_p = flowMsgCallbackFunc;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

        printf ( "Creating flow..." );
        if ( ( rc = solClient_session_createFlow ( flowProps,
                                                   session_p,
                                                   &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_createFlow() did not return SOLCLIENT_OK." );
            goto   sessionConnected;
        }
    printf ( "OK.\n" );

    /*************************************************************************
     * Wait for CTRL-C
     *************************************************************************/

    printf ( "Sending and Receiving, Ctrl-C to stop...\n" );

    /* Retrieve the temporary queue name from the Flow. 
     * NOTE: solClient_flow_getDestination()
     * can be used on temporary Queues or durable Flows. This sample
     * demonstrates both.
     */
    if ( ( rc = solClient_flow_getDestination ( flow_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_getDestination()" );
        goto sessionConnected;
    }

    while ( !gotCtlC ) {
        if ( ( rc = sendQueueMessage ( destination, session_p ) ) != SOLCLIENT_OK ) {
            sleepInSec ( 1 );   /* Let responses come in. */
            break;
        }
        sleepInSec ( 1 );
    }
    printf ( "Got Ctrl-C, cleaning up.\n" );

    /*************************************************************************
     * Cleanup
     *************************************************************************/

  sessionConnected:
    if ( endpointProvisioned ) {
        printf ( "Destroying flow.\n" );
        solClient_flow_destroy ( &flow_p );
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
