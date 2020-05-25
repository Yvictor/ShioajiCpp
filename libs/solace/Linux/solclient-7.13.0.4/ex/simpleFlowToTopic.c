
/** @example ex/simpleFlowToTopic.c
 */

/*
 * This sample shows the following:
 *    - Binding Flows to a Topic Endpoint (non-durable or durable)
 *    - Auto-acknowledgement
 *
 * For the durable Topic Endpoint, a durable Topic Endpoint called
 * 'my_sample_topicendpoint' must be provisioned on the appliance with at 
 * least 'Modify Topic' permissions. 
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

/*
 * fn main() 
 * param appliance ip address
 * param appliance username
 * param durability of the Topic Endpoint 
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

    solClient_opaqueFlow_pt flow_p;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    const char     *flowProps[20];
    int             propIndex;

    solClient_opaqueMsg_pt msg_p;                   /**< The message pointer */
    char            binMsg[] = COMMON_ATTACHMENT_TEXT;
    solClient_destination_t destination;
    /*
     * The Topic pointer, and memory for the temporary Topic used on non-durable enpoints.
     */
    char            tempTopic[SOLCLIENT_BUFINFO_MAX_TOPIC_SIZE];
    char           *topic_p;
    int             publishCount = 0;


    printf ( "\nsimpleFlowToTopic.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control C handling */
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
     * Create and connect a Session
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient sessions." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create a Flow
     *************************************************************************/

    flowFuncInfo.rxMsgInfo.callback_p = common_flowMessageReceivePrintMsgAndAckCallback;
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;

    propIndex = 0;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_TE;

    if ( commandOpts.usingDurable ) {
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
        flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
        flowProps[propIndex++] = COMMON_TESTDTE;
        topic_p = COMMON_MY_SAMPLE_TOPIC;
    } else {
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_DURABLE;
        flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
        /*
         * Demonstrate temporary Topic creation and use. Temporary or non-
         * temporary Topics can be used on non-durable endpoints.
         */
        if ( solClient_session_createTemporaryTopicName ( session_p, tempTopic, sizeof ( tempTopic ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_createTemporaryTopicName()" );
            goto sessionConnected;
        }
        topic_p = tempTopic;
    }
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_TOPIC;
    flowProps[propIndex++] = topic_p;

    /* Send an ack when the message has been received.  The default value is
     * to automatically acknowledge on return from the message receive callback
     * but it is recommended to use client aknowledgement when using flows.
     */
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;

    flowProps[propIndex] = NULL;

    if ( ( rc = solClient_session_createFlow ( flowProps,
                                               session_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_INFO,
                        "solClient_session_createFlow() did not return SOLCLIENT_OK " "after session create. rc = %d ", rc );
        goto sessionConnected;
    }

   /*************************************************************************
    * Publish
    *************************************************************************/

    if ( commandOpts.usingDurable ) {
        printf ( "Publishing 10 messages to durable Topic Endpoint %s, Ctrl-C to stop.....\n", COMMON_TESTDTE );
    } else {
        printf ( "Publishing 10 messages to a non-durable Topic Endpoint, Ctrl-C to stop.....\n" );
    }
    publishCount = 0;
    while ( !gotCtlC && publishCount < 10 ) {

        /*************************************************************************
         * MSG building
         *************************************************************************/

        /* Allocate a message. */
        if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_alloc()" );
            goto cleanupFlow;
        }
        /* Set the delivery mode for the message. */
        if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
            goto cleanupFlow;
        }
        /* Set a binary attachment and use it as part of the message. */
        if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p, binMsg, sizeof ( binMsg ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setBinaryAttachmentPtr()" );
            goto cleanupFlow;
        }

        destination.destType = SOLCLIENT_TOPIC_DESTINATION;
        destination.dest = topic_p;
        if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDestination()" );
            goto cleanupFlow;
        }

        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_send" );
            goto cleanupFlow;
        }

        if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_free()" );
            goto cleanupFlow;
        }
        publishCount++;

        sleepInSec ( 1 );
    }

    /*************************************************************************
     * Wait for CTRL-C
     *************************************************************************/

    if ( gotCtlC ) {
        printf ( "Got Ctrl-C, cleaning up\n" );
    }

    /************* Cleanup *************/

  cleanupFlow:
    if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_flow_destroy()" );
    }

    /*
     * Durable Topic Endpoints continue getting messages on the registered Topic
     * subscription if client applications do not unsubscribe.
     * Non-durable Topic Endpoints are be cleaned up automatically after client
     * applications dispose the Flows bound to them.
     *
     * The following code block demonstrates how to unsubscribe or remove a subscribed
     * Topic on the durable Topic Endpoint.
     * Two conditions must be met:
     * - The durable Topic Endpoint must have 'Modify Topic' permission enabled
     *   (at least).
     * - No flows are currently bound to the durable Topic Endpoint in question.
     */
    if ( commandOpts.usingDurable ) {
        printf ( "About to unsubscribe from durable Topic Endpoint %s\n", COMMON_TESTDTE );
        if ( ( rc = solClient_session_dteUnsubscribe ( session_p, COMMON_TESTDTE, "correlation_tag" ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_dteUnsubscribe()" );
        }
        /* Sleep to allow unsubscribe to finish. */
        sleepInSec ( 1 );
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
