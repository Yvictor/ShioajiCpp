
/** @example ex/messageSelectorsOnQueue.c
 */

/*
 * This sample demonstrates how to:
 * - Create and bind a Flow to a temporary Queue with a message selector on a 
 *   user-defined property.
 * - Publish a number of Guaranteed messages with the given user-defined
 *   property to the temporary Queue.
 * - Show that, messages matching the registered message selector are delivered
 *   to the temporary Queue Flow.
 *
 * Copyright 2009-2020 Solace Corporation. All rights reserved.
 */

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "common.h"


/*
 * Publish messages to the Queue. 
 */
static void
pubMsg ( solClient_opaqueSession_pt session_p, const char *destinationNameStr_p, const char *pastaStr_p )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;
    solClient_opaqueContainer_pt userPropMap_p;
    char            payload[] = COMMON_ATTACHMENT_TEXT;
    solClient_log ( SOLCLIENT_LOG_DEBUG, "About to publish\n" );

    /* Allocate the message. */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        return;
    }

    /* Set the message delivery mode. */
    if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        return;
    }

    /*
     * Add the destination Topic to the message. A non-durable Queue,
     * with a name obtained from the API is being used.
     */
    destination.destType = SOLCLIENT_QUEUE_DESTINATION;
    destination.dest = destinationNameStr_p;
    if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMsg;
    }

    /* Create a map to hold user-defined property fields. */
    if ( ( rc = solClient_msg_createUserPropertyMap ( msg_p, &userPropMap_p, 100 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_createUserPropertyMap()" );
        goto freeMsg;
    }

    /* Add a custom header field. */
    if ( ( rc = solClient_container_addString ( userPropMap_p, pastaStr_p, "pasta" ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_addString()" );
        goto freeMsg;
    }
    if ( ( rc = solClient_container_closeMapStream ( &userPropMap_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_closeMapStream()" );
        goto freeMsg;
    }

    /* Set the binary attachment. */
    if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p, payload, sizeof ( payload ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setBinaryAttachment()" );
        goto freeMsg;
    }

    /* 
     * After building a message, use sendMsg() to publish it. Malformed messages
     * will result in a session event callback call with the error.
     */
    if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_sendMsg()" );
    }

  freeMsg:
    /* Finally, free the allocated message. */
    if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
    }

}                               //End pubMsg


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

    solClient_opaqueSession_pt session_p;

    solClient_opaqueFlow_pt flow_p = NULL;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;

    const char     *flowProps[20];
    int             propIndex;

    solClient_destination_t flowDest;

    printf ( "\nmessageSelectorsOnQueue.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

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
     * Initialize API and setup logging level
     *************************************************************************/

    /* solClient needs to be initialized before any other API calls are made. */
    if ( ( rc = solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_initialize()" );
        goto notInitialized;
    }

    common_printCCSMPversion (  );

    /* 
     * Standard logging levels can be set independently for the API and the
     * application. In this case, the ALL category is used to set the log level 
     * for both at the same time.
     */
    solClient_log_setFilterLevel ( SOLCLIENT_LOG_CATEGORY_ALL, commandOpts.logLevel );

    /*************************************************************************
     * Create a Context
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient context" );

    /* 
     * Create a Context, and specify that the Context thread be created 
     * automatically instead of having the application create its own
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

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient Sessions." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceiveCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create a Flow
     *************************************************************************/

    flowFuncInfo.rxMsgInfo.callback_p = common_flowMessageReceivePrintMsgAndAckCallback;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    /* Create a Flow. */
    propIndex = 0;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    /* Set Acknowledge mode to CLIENT_ACK */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

    /* Let the API generate a queueName */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
    flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
    /* In a started state. */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_START_STATE;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    /* The selector. */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_SELECTOR;
    flowProps[propIndex++] = "pasta='rotini' OR pasta='farfalle'";
    flowProps[propIndex++] = NULL;


    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_INFO,
                        "solClient_session_createFlow() did not return SOLCLIENT_OK after session connect. rc = %d ", rc );
        goto sessionConnected;
    }

    if ( ( rc = solClient_flow_getDestination ( flow_p, &flowDest, sizeof ( flowDest ) ) ) == SOLCLIENT_OK ) {
        printf ( "Created Flow to receive messages sent to %s\n", flowDest.dest );
    } else {
        common_handleError ( rc, "Unable to retrieve Flow Destination" );
    }

    /*************************************************************************
     * Wait for messages
     *************************************************************************/

    printf ( "Waiting for messages.....Expecting two messages to match the selector" );

    /* Send message */
    pubMsg ( session_p, flowDest.dest, "macaroni" );
    pubMsg ( session_p, flowDest.dest, "fettuccini" );
    pubMsg ( session_p, flowDest.dest, "farfalle" );    /* Should match */
    pubMsg ( session_p, flowDest.dest, "fiori" );
    pubMsg ( session_p, flowDest.dest, "rotini" );      /* Should match */
    pubMsg ( session_p, flowDest.dest, "penne" );
    sleepInSec ( 5 );           // wait for 5 seconds and exit

    /*************************************************************************
     * Cleanup
     *************************************************************************/

 sessionConnected:
    /*
     * solClient_cleanup() will destroy all objects. This disconnect Sessions 
     * with the appliance which will implicitly unbind all flows. However without an
     * explicit unbind, temporary endpoints will linger for 60 seconds. To ensure
     * temporary endpoints are removed immmediately, applications should explicitly
     * destroy Flows and not rely solely on solClient_cleanup().
     */
    if ( flow_p != NULL ) {
        if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_flow_destroy()" );
        }
    }


    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

  cleanup:
    /* Cleanup solClient */
    if ( ( rc = solClient_cleanup (  ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cleanup()" );
    }
    goto notInitialized;

  notInitialized:
    /* Nothing to do - just exit */

    return 0;
}

//End main()
