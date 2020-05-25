/** @example ex/transactions.c
 */

/*
 * This sample uses a simple request/reply scenario to show the use of transactions.
 *
 *    TransactedRequestor: A transacted session and a transacted consumer flow are created
 *                         for the TransactedRequestor. The TransactedRequestor sends a
 *                         request message and commits the transaction. It then waits up to 
 *                         10s for a reply message. It commits the transaction again after it
 *                         receives a reply message.
 *                         
 *    TransactedReplier:   A transacted session and a transacted consumer flow are created 
 *                         for the TransactedReplier. When the TransactedReplier receives 
 *                         a request message, it sends a reply message and then commits the
 *                         transaction. 
 *             
 *  |---------------------|  -------- RequestTopic ---------> |--------------------|
 *  | TransactedRequestor |                                   | TransactedReplier  |
 *  |---------------------|  <-------- ReplyQueue ----------  |--------------------|
 *
 *
 * Copyright 2013-2020 Solace Corporation. All rights reserved.
 */

#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"


#define MY_SAMPLE_REQUEST_TE "my_sample_request_te"

/*
 * Replier consumer flow Rx message callback
 *   It sends a reply for a received request and then commits the transaction.
 */
static solClient_rxMsgCallback_returnCode_t
replierFlowRxMsgCallbackFunc (
    solClient_opaqueFlow_pt opaqueFlow_p,
    solClient_opaqueMsg_pt msg_p,
    void *user_p )
{
    solClient_returnCode_t               rc;
    const char                           *senderId_p = NULL;
    solClient_destination_t              replyTo;
    solClient_opaqueMsg_pt               replyMsg_p = NULL;
    solClient_opaqueTransactedSession_pt transactedSession_p = NULL;    

    /* Get the SenderId */
    rc = solClient_msg_getSenderId ( msg_p, &senderId_p );
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_getSenderId()" );
        goto done;
    }
    
    /* Get ReplyTo address */
    rc = solClient_msg_getReplyTo(  msg_p, &replyTo, sizeof(replyTo));
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_getReplyTo()");
        goto done;
    }

    /* Get the Transacted Session pointer */
    rc = solClient_flow_getTransactedSession(opaqueFlow_p, &transactedSession_p);
    if (rc != SOLCLIENT_OK) {
        common_handleError ( rc, "solClient_flow_getTransactedSession()");
        goto done;
    }

    printf("Replier receives a request message from '%s'. It sends a reply message and then commits the transaction.\n", senderId_p);

    /* Create a reply message */
    rc = solClient_msg_alloc ( &replyMsg_p );
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        goto done;
    }

    /* Set the reply message Destination */
    rc = solClient_msg_setDestination ( replyMsg_p, &replyTo, sizeof ( replyTo ));
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMsg;
    }

    /* Set the reply message Delivery Mode */
    rc = solClient_msg_setDeliveryMode ( replyMsg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT);
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto freeMsg;
    }

    /* Send the reply message */
    rc = solClient_transactedSession_sendMsg (transactedSession_p, replyMsg_p);
    if (rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_transactedSession_sendMsg()" );
        goto freeMsg;
    }

    rc =  solClient_transactedSession_commit(transactedSession_p);
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_transactedSession_commit()" );
        goto freeMsg;
    }

 freeMsg:
    rc = solClient_msg_free ( &replyMsg_p);
    if ( rc != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
    }
 done:
    return SOLCLIENT_CALLBACK_OK;
}


/*
 * fn main() 
 * param appliance ip address
 * param appliance username
 * param request topic
 * 
 * The entry point to the application.
 */
int
main ( int argc, char *argv[] )
{

    struct commonOptions                 commandOpts;
    solClient_returnCode_t               rc = SOLCLIENT_OK;
    solClient_opaqueContext_pt           context_p;
    solClient_context_createFuncInfo_t   contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;
    solClient_opaqueSession_pt           session_p = NULL;
    solClient_opaqueTransactedSession_pt requestorTransactedSession_p = NULL;
    solClient_opaqueTransactedSession_pt replierTransactedSession_p = NULL;
    solClient_opaqueFlow_pt              requestorFlow_p = NULL;
    solClient_opaqueFlow_pt              replierFlow_p = NULL;
    solClient_flow_createFuncInfo_t      flowFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    const char                           *flowProps[100];
    int                                  propIndex = 0;
    const char                           senderId[] = "Requestor";
    solClient_opaqueMsg_pt               requestMsg_p = NULL;  
    solClient_opaqueMsg_pt               replyMsg_p = NULL;  
    solClient_destination_t              destination;
    solClient_destination_t              replyToAddr;
    solClient_bool_t                     endpointProvisioned = FALSE;
    const char                           *provProps[20];
 
    printf ( "transactions.c (Copyright 2013-2020 Solace Corporation. All rights reserved.)\n" );

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
                                           &context_p,
                                           &contextFuncInfo,
                                           sizeof ( contextFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto cleanup;
    }


    /*************************************************************************
     * Create and connect a Session
     *************************************************************************/
    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback,
                                                 NULL, 
                                                 &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Ensure Transacted Session is enabled on appliance 
     *************************************************************************/
    if ( !solClient_session_isCapable ( session_p, SOLCLIENT_SESSION_CAPABILITY_TRANSACTED_SESSION ) ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Transacted session not supported." );
        goto sessionConnected;
    }

    /*************************************************************************
     * Ensure Endpoint provisioning is enabled on appliance
     *************************************************************************/
    if ( !solClient_session_isCapable ( session_p, SOLCLIENT_SESSION_CAPABILITY_ENDPOINT_MANAGEMENT ) ) {

        solClient_log ( SOLCLIENT_LOG_ERROR, "Endpoint management not supported." );
        goto sessionConnected;
    }

    /****************************************************************
     * Create a Transacted Session for TransactedReplier
     ***************************************************************/
    if ((rc = solClient_session_createTransactedSession(NULL,  
                                                        session_p,
                                                        &replierTransactedSession_p,
                                                        NULL)) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_createTransactedSession()" );
        goto sessionConnected;
    }
    /*************************************************************************
     * Provision a Topic Endpoint for the request topic on appliance
     *************************************************************************/
    propIndex = 0;
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_TE;
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
    provProps[propIndex++] = MY_SAMPLE_REQUEST_TE; 
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_PERMISSION;
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PERM_MODIFY_TOPIC;
    provProps[propIndex++] = SOLCLIENT_ENDPOINT_PROP_QUOTA_MB;
    provProps[propIndex++] = "100";
    provProps[propIndex++] = NULL;

    if ( ( rc = solClient_session_endpointProvision ( provProps,
                                                      session_p,
                                                      SOLCLIENT_PROVISION_FLAGS_WAITFORCONFIRM,
                                                      NULL, NULL, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_endpointProvision()");
        goto sessionConnected;
    }

    endpointProvisioned = TRUE;

    /*************************************************************************
     * Create a Transacted Consumer Flow with a Rx message callback for TransactedReplier
     *************************************************************************/
    flowFuncInfo.rxMsgInfo.callback_p = replierFlowRxMsgCallbackFunc;
    flowFuncInfo.rxMsgInfo.user_p = NULL;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_TE;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    flowProps[propIndex++] = MY_SAMPLE_REQUEST_TE;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_TOPIC;
    flowProps[propIndex++] = commandOpts.destinationName;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex] = NULL;

    if ( ( rc = solClient_transactedSession_createFlow ( flowProps,
                                                         replierTransactedSession_p,
                                                         &replierFlow_p,
                                                         &flowFuncInfo,
                                                         sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError(rc, "solClient_transactedSession_createFlow()");
        goto sessionConnected;
    }

    /*************************************************************************
     * Create a Transacted Session for TransactedRequestor
     ***********************************************************************/
    if ((rc = solClient_session_createTransactedSession(NULL,  
                                                        session_p,
                                                        &requestorTransactedSession_p,
                                                        NULL)) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_createTransactedSession()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Create a Temporary Queue and a Transacted Consumer Flow without specifying a 
     * Rx message callback for the TransactedRequestor
     *************************************************************************/
    flowFuncInfo.rxMsgInfo.callback_p = NULL; /* No Rx message callback */
    flowFuncInfo.rxMsgInfo.user_p = NULL;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
    flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;

    flowProps[propIndex] = NULL;

    /* Create a transacted consumer flow. */
    if ( ( rc = solClient_transactedSession_createFlow ( flowProps,
                                                         requestorTransactedSession_p,
                                                         &requestorFlow_p,
                                                         &flowFuncInfo,
                                                         sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError(rc, "solClient_transactedSession_createFlow()");
        goto sessionConnected;
    }


    /* Allocate a request Message */
    if ( ( rc = solClient_msg_alloc ( &requestMsg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        goto sessionConnected;
    }

    /* Set the request message Delivery Mode */
    if ( ( rc = solClient_msg_setDeliveryMode ( requestMsg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto freeMsg;
    }

    /* Set the request message SenderId */
    if ( ( rc = solClient_msg_setSenderId ( requestMsg_p, senderId ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto freeMsg;
    }

    /* Set the request message Destination */
    destination.destType = SOLCLIENT_TOPIC_DESTINATION;
    destination.dest = commandOpts.destinationName;
    if ( ( rc = solClient_msg_setDestination ( requestMsg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMsg;
    }
    
    /* Retrieve a queue name for the Temporary Queue */
    if ( ( rc = solClient_flow_getDestination (requestorFlow_p, &replyToAddr, sizeof(replyToAddr))) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_getDestination()" );
        goto freeMsg;
    }

    /* set request message ReplyTo address */
    if ( ( rc = solClient_msg_setReplyTo ( requestMsg_p, &replyToAddr, sizeof ( replyToAddr ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMsg;
    }
    
    /* Send the request message */
    if ( ( rc = solClient_transactedSession_sendMsg ( requestorTransactedSession_p, requestMsg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_transactedSession_sendMsg()" );
        goto freeMsg;
    }
    printf ( "Requestor sends a request message on topic '%s' and then commits the transaction.\n", commandOpts.destinationName );

    /* Commit the Transaction for the request message */
    if ( (rc = solClient_transactedSession_commit(requestorTransactedSession_p) )!= SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_transactedSession_commit" );
        goto freeMsg;
    }    

    /*
     * Wait up to 10s for a reply message.
     * This message receiving API is only supported for transacted consumer flows.
     */
    if ( (rc = solClient_flow_receiveMsg(requestorFlow_p, &replyMsg_p, 10000) )!= SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_receiveMsg()" );
        goto freeMsg;
    }

    printf ( "Requestor receives a reply message and commits the transaction.\n");

    /* Free the received message */
    if ( ( rc = solClient_msg_free ( &replyMsg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
        goto freeMsg;
     }
    
    /* Commit the transaction for the reply message */
    if ( (rc = solClient_transactedSession_commit(requestorTransactedSession_p) )!= SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_transactedSession_commit" );
    }

    /************* Cleanup *************/
 freeMsg:
    if ( ( rc = solClient_msg_free ( &requestMsg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
    }

  sessionConnected:
    if (replierTransactedSession_p != NULL) {
        if ( ( rc = solClient_transactedSession_destroy( &replierTransactedSession_p ) ) != SOLCLIENT_OK ) {
          common_handleError ( rc, "solClient_transactedSession_destroy()" );
        }
    }
    if (requestorTransactedSession_p != NULL) {
        if ( ( rc = solClient_transactedSession_destroy( &requestorTransactedSession_p ) ) != SOLCLIENT_OK ) {
          common_handleError ( rc, "solClient_transactedSession_destroy()" );
        }
    }
    if (endpointProvisioned) {
        /*
         * Remove the Endpoint from the appliance
         */
        if ( ( rc = solClient_session_endpointDeprovision ( provProps,
                                                            session_p,
                                                            SOLCLIENT_PROVISION_FLAGS_WAITFORCONFIRM, NULL ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_endpointDeprovision()");
        }
    }

    /* Disconnect the Session */
    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

  cleanup:
    /* Cleanup */
    if ( ( rc = solClient_cleanup (  ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cleanup()" );
    }
    goto notInitialized;

  notInitialized:
    return 0;
}  
