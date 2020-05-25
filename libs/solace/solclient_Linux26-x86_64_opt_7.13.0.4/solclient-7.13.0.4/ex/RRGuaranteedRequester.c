/** @example ex/RRGuaranteedRequester.c
 */

/*
 * This sample shows how to implement a Requester for guaranteed Request-Reply messaging, where 
 *
 *    RRGuaranteedRequester: A message Endpoint that sends a guaranteed request message and waits
 *                           to receive a reply message as a response.
 *    RRGuaranteedReplier:   A message Endpoint that waits to receive a request message and responses
 *                           to it by sending a guaranteed reply message.
 *             
 *  |-----------------------|  -- RequestQueue/RequestTopic --> |----------------------|
 *  | RRGuaranteedRequester |                                   | RRGuaranteedReplier  |
 *  |-----------------------|  <-------- ReplyQueue ----------  |----------------------|
 *
 * Notes: the RRGuaranteedReplier supports request queue or topic formats, but not both at the same time.
 *
 * Copyright 2013-2020 Solace Corporation. All rights reserved.
 */


#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"
#include "RRcommon.h"

/**
 * @struct requestMessageInfo
 * The structure used for request message info.
*/
typedef struct requestMessageInfo
{
    BOOL                           replyReceived;
} requestMessageInfo_t, *requestMessageInfo_pt; 


/*
 * Received reply message handling code
 */
static solClient_rxMsgCallback_returnCode_t
flowMsgCallbackFunc ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_returnCode_t rc;
    requestMessageInfo_pt  requestInfo_p = (requestMessageInfo_pt)user_p;
    solClient_bool_t       resultOk;
    double      result;
    solClient_opaqueContainer_pt stream_p;

    requestInfo_p->replyReceived = TRUE;

    /*
     * Get the results in the binary attachment.
     */
    if ( ( rc = solClient_msg_getBinaryAttachmentStream ( msg_p, &stream_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_getBinaryAttachmentStream()" );
        goto done;
    }
    /* Get the Boolean value indicating if the operation was success or not. */
    if ( ( rc = solClient_container_getBoolean ( stream_p, &resultOk, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_getBoolean() for operation" );
        goto done;
    }
    
    if (resultOk == FALSE) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Received reply message with failed status." );
        goto done;
    }

    /* Get the operation result value. */
    if ( ( rc = solClient_container_getDouble ( stream_p, &result, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_getDouble() for operation" );
        goto done;
    }
    printf("Received reply message, result = %f\n", result );

 done:
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

    solClient_opaqueSession_pt session_p = NULL;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    const char     *sessionProps[50];

    solClient_opaqueFlow_pt flow_p = NULL;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    const char     *flowProps[20];

    int             propIndex = 0;

    solClient_opaqueMsg_pt msg_p = NULL;  

    solClient_destination_t destination;
    solClient_destination_t replyToAddr;
    solClient_opaqueContainer_pt stream_p;
    solClient_int32_t operand1 = 9;
    solClient_int32_t operand2 = 5;
    RR_operation_t     operation = firstOperation;
    int             waitInSec = 10; /* 10 sec */

    requestMessageInfo_t  requestInfo;
    char                  requestQueue_a[256];
    char                  *positionalParms = "\tQUEUE               Guaranteed Message Queue.\n";

    printf ( "RRGuaranteedRequester.c (Copyright 2013-2020 Solace Corporation. All rights reserved.)\n" );

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
     * Create a Flow and a temporary reply Queue
     *************************************************************************/
    flowFuncInfo.rxMsgInfo.callback_p = flowMsgCallbackFunc;
    flowFuncInfo.rxMsgInfo.user_p = &requestInfo;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
    flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;

    flowProps[propIndex] = NULL;

    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_INFO,
                        "solClient_session_createFlow() did not return SOLCLIENT_OK " "after session create. rc = %d ", rc );
        goto sessionConnected;
    }

    /*************************************************************************
     * Request Message
     *************************************************************************/
    if (commandOpts.destinationName[0] == '\0') {
        printf ( "Send request messages to queue '%s', Ctrl-C to stop.....\n", requestQueue_a );
    }
    else {
        printf ( "Send request messages to topic '%s', Ctrl-C to stop.....\n", commandOpts.destinationName );
    }

    /* Allocate a message. */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
      common_handleError ( rc, "solClient_msg_alloc()" );
      goto cleanupFlow;
    }

    /* Note: A bad operation is purposely sent in this example (lastOperation + 1). */
    operation = firstOperation;
    while ( !gotCtlC && operation <= lastOperation + 1 ){

         /* Reset the request info */
        requestInfo.replyReceived = FALSE;

        /* reset request message. */ 
        waitInSec = 10; /* 10 sec */
        if ( ( rc = solClient_msg_reset ( msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_reset()" );
            goto freeMsg;
        }

        /* Set the delivery mode for the message. */
        if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
            goto freeMsg;
        }

        /* Create a stream in the binary attachment part of the message. */
        if ( ( rc = solClient_msg_createBinaryAttachmentStream ( msg_p, &stream_p, 100 ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_createBinaryAttachmentStream()" );
            goto freeMsg;
        }

        /* Put the operation, operand1, operand2 into the stream. */
        if ( ( rc = solClient_container_addInt8 ( stream_p, ( solClient_int8_t ) operation, NULL ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_container_addInt8()" );
            goto freeMsg;
        }
        if ( ( rc = solClient_container_addInt32 ( stream_p, operand1, NULL ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_container_addInt32()" );
            goto freeMsg;
        }
        if ( ( rc = solClient_container_addInt32 ( stream_p, operand2, NULL ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_container_addInt32()" );
            goto freeMsg;
        }

        /* set destination. */
        if (commandOpts.destinationName[0] == '\0') {
            /* queue destination. */
            destination.destType = SOLCLIENT_QUEUE_DESTINATION;
            destination.dest = requestQueue_a;
            if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
                common_handleError ( rc, "solClient_msg_setDestination()" );
                goto cleanupFlow;
            }
        }
        else {
            /* topic destination. */
            destination.destType = SOLCLIENT_TOPIC_DESTINATION;
            destination.dest = commandOpts.destinationName;
            if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
                common_handleError ( rc, "solClient_msg_setDestination()" );
                goto cleanupFlow;
            }
        }

        /*
         * Retrieve the temporary queue name from the Flow.
         */
        if ( ( rc = solClient_flow_getDestination ( flow_p, &replyToAddr, sizeof ( replyToAddr ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_flow_getDestination()" );
            goto freeMsg;
        }
        /* set the replyTo address. */
        if ( ( rc = solClient_msg_setReplyTo ( msg_p, &replyToAddr, sizeof ( replyToAddr ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDestination()" );
            goto freeMsg;
        }

        /* Send request message. */
        if ( operation <= lastOperation ) {
            printf ( "Sending request for '%d %s %d'\n", operand1, RR_operationToString ( operation ), operand2 );
        }
        else {
           printf ( "Sending request for a bad operation '%d %s %d', expect an APP error\n",
                    operand1, RR_operationToString ( operation ), operand2 );
        }
        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_send" );
            goto freeMsg;
        }

        /* Wait until a reply message or Ctrl-C is received. */ 
        while ((!requestInfo.replyReceived)&&(!gotCtlC) &&(waitInSec>0)) {
            sleepInSec ( 1 );
            waitInSec--;
        }
        if (waitInSec == 0) {
            solClient_log ( SOLCLIENT_LOG_ERROR, "Request message timeout." );
            goto freeMsg;
        }
        operation++;
    }

    /*************************************************************************
     * Wait for CTRL-C
     *************************************************************************/
    if ( gotCtlC ) {
        printf ( "Got Ctrl-C, cleaning up\n" );
    }

    /************* Cleanup *************/
 freeMsg:
    if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
      common_handleError ( rc, "solClient_msg_free()" );
     }
 
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
    goto notInitialized;

  notInitialized:

    return 0;
}  
