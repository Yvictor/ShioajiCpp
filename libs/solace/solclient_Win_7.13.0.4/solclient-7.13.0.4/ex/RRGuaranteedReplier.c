/** @example ex/RRGuaranteedReplier.c
 */

/*
 * This sample shows how to implement a Replier for guaranteed Request-Reply messaging, where 
 *
 *    RRGuaranteedRequester: A message Endpoint that sends a guaranteed request message and waits to receive
 *                           a reply message as a response.
 *    RRGuaranteedReplier:   A message Endpoint that waits to receive a request message and responses to it 
 *                           by sending a guaranteed reply message.
 *             
 *  |-----------------------|  ---RequestQueue/RequestTopic --> |----------------------|
 *  | RRGuaranteedRequester |                                   | RRGuaranteedReplier  |
 *  |-----------------------|  <-------- ReplyQueue ----------  |----------------------|
 *
 * Notes: the RRGuaranteedReplier supports request queue or topic formats, but not both at the same time.
 *
 * Copyright 2013-2020 Solace Corporation. All rights reserved.
 */

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"
#include "RRcommon.h"


/*
 * Received request message handling code
 */
static          solClient_rxMsgCallback_returnCode_t
flowMsgCallbackFunc ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_returnCode_t  rc;
    solClient_destination_t replyTo;
    solClient_opaqueMsg_pt  replyMsg_p = NULL;
    solClient_opaqueContainer_pt stream_p;
    solClient_opaqueContainer_pt replyStream_p;
    solClient_bool_t resultOk = FALSE;
    solClient_int8_t operation;
    solClient_int32_t operand1;
    solClient_int32_t operand2;
    double result;

    solClient_opaqueSession_pt  session_p = (solClient_opaqueSession_pt)user_p;

    /* Get reply queue address. */
    rc = solClient_msg_getReplyTo(  msg_p, &replyTo, sizeof(replyTo));
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_getReplyTo()");
        return SOLCLIENT_CALLBACK_OK;
    }

    /*
     * Get the operator, operand1 and operand2 from the stream in the binary
     * attachment.
     */
    if ( ( rc = solClient_msg_getBinaryAttachmentStream ( msg_p, &stream_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_getBinaryAttachmentStream()" );
        goto createReply;
    }
    /* Get the operator, operand1 and operand2 from the stream. */
    if ( ( rc = solClient_container_getInt8 ( stream_p, &operation, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_getInt8() for operation" );
        goto createReply;
    }
    if ( ( rc = solClient_container_getInt32 ( stream_p, &operand1, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_getInt32() for operand1" );
        goto createReply;
    }
    if ( ( rc = solClient_container_getInt32 ( stream_p, &operand2, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_getInt32() for operand2" );
        goto createReply;
    }

    /* Do the requested calculation. */
    switch ( operation ) {
        case plusOperation:
            result = (double)operand1 + (double)operand2;
            resultOk = TRUE;
            break;
        case minusOperation:
            result = (double)operand1 - (double)operand2;
            resultOk = TRUE;
            break;
        case timesOperation:
            result = (double)operand1 * (double)operand2;
            resultOk = TRUE;
            break;
        case divideOperation:
            if (operand2 != 0) {
                result = (double)operand1 / (double)operand2;
                resultOk = TRUE;
            }
            break;
        default:
            break;
    }

  createReply:
    if ( resultOk ) {
        printf( "  Received request for %d %s %d, sending reply with result %f. \n",
                operand1, RR_operationToString ( operation ), operand2, result );
    } else {
        printf( "  Received request for %d %s %d, sending reply with a failure status. \n",
                operand1, RR_operationToString ( operation ), operand2 );
    }

    /*
     * Allocate a message to construct the reply, and put in the status and result in a
     * stream.
     */
    if ( ( rc = solClient_msg_alloc ( &replyMsg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        return SOLCLIENT_CALLBACK_OK;
    }
    if ( ( rc = solClient_msg_createBinaryAttachmentStream ( replyMsg_p, &replyStream_p, 32 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_createBinaryAttachmentStream()" );
        goto freeMsg;
    }
    if ( ( rc = solClient_container_addBoolean ( replyStream_p, resultOk, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_addBoolean()" );
        goto freeMsg;
    }
    if ( resultOk ) {
        if ( ( rc = solClient_container_addDouble ( replyStream_p, result, NULL ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_container_addDouble()" );
            goto freeMsg;
        }
    }

    /* Set the delivery mode for the reply message. */
    if ( ( rc = solClient_msg_setDeliveryMode ( replyMsg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto freeMsg;
    }

    /* Set the reply message destination. */
    if ( ( rc = solClient_msg_setDestination ( replyMsg_p, &replyTo, sizeof ( replyTo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMsg;
    }

    /* Send the reply message. */ 
    if ( ( rc = solClient_session_sendMsg ( session_p, replyMsg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_send" );
        goto freeMsg;
    }
    
 freeMsg:
    if ( ( rc = solClient_msg_free ( &replyMsg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
    }
    return SOLCLIENT_CALLBACK_OK;
}


/*
 * fn main() 
 * param appliance ip address
 * param appliance username
 * param request queue or topic
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
    const char     *sessionProps[50];

    solClient_opaqueFlow_pt flow_p;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    const char     *flowProps[20];

    const char     *provProps[20];
    char           requestQueue_a[256];

    int             propIndex;
    solClient_bool_t endpointProvisioned = FALSE;
    solClient_errorInfo_pt errorInfo_p;

    char           *positionalParms = "\tQUEUE               Guaranteed Message Queue.\n";

    printf ( "\nRRGuaranteedReplier.c (Copyright 2013-2020 Solace Corporation. All rights reserved.)\n" );

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
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, positionalParms ) == 0 ) {
        exit (1);
    }

    /*
     * see if a queue name has been requested, this is mutually exclusive 
     * with a topic destination.  However at leat one must be set.
     */
    if (optind < argc) {
        if (commandOpts.destinationName[0] != '\0') {
            printf("%s does not support topic ('-t, --topic) and queue name (%s) at the same time\n",
                argv[0], argv[optind]);
            exit(1);
        }
        strncpy(requestQueue_a, argv[optind], sizeof(requestQueue_a));
    }
    else {
        if (commandOpts.destinationName[0] == '\0') {
            printf("%s must specify either a topic ('-t, --topic) or a queue name argument\n",
                argv[0]);
            exit(1);
        }
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

    sessionFuncInfo.rxMsgInfo.callback_p = common_messageReceivePrintMsgCallback;
    sessionFuncInfo.rxMsgInfo.user_p = NULL;
    sessionFuncInfo.eventInfo.callback_p = common_eventCallback;
    sessionFuncInfo.eventInfo.user_p = NULL;;

    propIndex = 0;
    if ( commandOpts.targetHost[0] != (char) 0 ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = commandOpts.targetHost;
    }

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL;
    sessionProps[propIndex++] = ( commandOpts.enableCompression ) ? "9" : "0";

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_CONNECT_RETRIES;
    sessionProps[propIndex++] = "3";

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES;
    sessionProps[propIndex++] = "3";

    /*
     * Note: Reapplying subscriptions allows Sessions to reconnect after failure and
     * have all their subscriptions automatically restored. For Sessions with many
     * subscriptions, this can increase the amount of time required for a successful
     * reconnect.
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    /*
     * Note: Including meta data fields such as sender timestamp, sender ID, and sequence 
     * number can reduce the maximum attainable throughput as significant extra encoding/
     * decodingis required. This is true whether the fields are autogenerated or manually
     * added.
     */

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_SEND_TIMESTAMPS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_SENDER_ID;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_SEQUENCE_NUMBER;
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

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = commandOpts.username;
    
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = commandOpts.password;

    if ( commandOpts.useGSS ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME;
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME_GSS_KRB;
    }

    sessionProps[propIndex] = NULL;

    /*
     * Create a session.
     */
    if ( ( rc = solClient_session_create ( sessionProps,
                                           context_p,
                                           &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto cleanup;
    }

    /*
     * Connect the session.
     */
    if ( ( rc = solClient_session_connect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
         goto cleanup;
    }

    /*************************************************************************
     * Ensure the endpoint provisioning is supported 
     *************************************************************************/
    if ( !solClient_session_isCapable ( session_p, SOLCLIENT_SESSION_CAPABILITY_ENDPOINT_MANAGEMENT ) ) {

        solClient_log ( SOLCLIENT_LOG_ERROR, "Endpoint management not supported." );
        goto sessionConnected;
    }

    /*************************************************************************
     * Provision a durable queue or topic endpoint on appliance
     *************************************************************************/
    propIndex = 0;
    if (commandOpts.destinationName[0] == '\0' ) {
        provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
        provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_QUEUE;
        provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
        provProps[propIndex++] = requestQueue_a;
    }
    else {
        provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
        provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_TE;
        provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
        provProps[propIndex++] = MY_SAMPLE_REQUEST_TE; 
    }
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_PERMISSION;
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PERM_MODIFY_TOPIC;
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_QUOTA_MB;
    provProps[propIndex++] = "100";
    provProps[propIndex++] = NULL;

    /* Try to provision the endpoint. */
    if ( ( rc = solClient_session_endpointProvision ( provProps,
                                                      session_p,
                                                      SOLCLIENT_PROVISION_FLAGS_WAITFORCONFIRM,
                                                      NULL, NULL, 0 ) ) != SOLCLIENT_OK ) {
        errorInfo_p = solClient_getLastErrorInfo (  );
        if ( errorInfo_p != NULL ) {
            solClient_log ( SOLCLIENT_LOG_WARNING,
                            "solClient_session_endpointProvision() failed subCode (%d:'%s')",
                            errorInfo_p->subCode, solClient_subCodeToString ( errorInfo_p->subCode ) );
        }
        goto sessionConnected;
    }

    endpointProvisioned = TRUE;

    /*************************************************************************
     * Create a Flow
     *************************************************************************/
    flowFuncInfo.rxMsgInfo.callback_p = flowMsgCallbackFunc;
    flowFuncInfo.rxMsgInfo.user_p = session_p;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    if (commandOpts.destinationName[0] == '\0') {
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
        flowProps[propIndex++] = requestQueue_a;
    }
    else {
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_TE;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
        flowProps[propIndex++] = MY_SAMPLE_REQUEST_TE;

        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_TOPIC;
        flowProps[propIndex++] = commandOpts.destinationName;
    }
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    flowProps[propIndex] = NULL;

    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_INFO,
                        "solClient_session_createFlow() did not return SOLCLIENT_OK " "after session create. rc = %d ", rc );
        goto sessionConnected;
    }

    /*************************************************************************
     * Serve requests, CTRL-C to stop
     *************************************************************************/
    if (commandOpts.destinationName[0] == '\0') {
        printf ( "Serving requests on queue '%s', Ctrl-C to stop.....\n", requestQueue_a );
    }
    else {
        printf ( "Serving requests on topic '%s', Ctrl-C to stop.....\n", commandOpts.destinationName );
    }
    while ( !gotCtlC ) {
        sleepInSec ( 1 );
    }
    printf ( "Got Ctrl-C, cleaning up\n" );


    /************* Cleanup *************/
    if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_destroy()" );
    }

  sessionConnected:
    if (endpointProvisioned) {
        /*
         * Remove the endpoint from the appliance.
         */
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

    return 0;
}


