
/** @example ex/noLocalPubSub.c 
 */

/*
 * This sample demonstrates:
 *  - Subscribing to a Topic for Direct messages on a Session with No Local delivery enabled.
 *  - Creating a Flow to a Queue with no Local Delivery enabled on the Flow, but not on the Session.
 *  - Publish a message to the Direct message on each Session, and verify that it is not delivered locally.
 *  - Publish a message to the Queue on each Session, and verify that it is not delivered locally.
 *
 * This sample demonstrates the use of the NO_LOCAL Session and flow property. With
 * this property enabled, messages are not received on the publishing Session, even with a 
 * Topic or Flow match.
 *
 * Copyright 2010-2020 Solace Corporation. All rights reserved.
 */

/*****************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *****************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"


/*****************************************************************************
 * messageReceiveCallback
 *
 * The message callback is invoked for each Direct message received by
 * the Session. The user pointer is expected to be a pointer to a 32-bit 
 * counter that is incremented for each received message.
 * 
 * Message callback code is executed within the API thread which means that
 * it should deal with the message quickly or queue the message for further
 * processing in another thread.
 * 
 *****************************************************************************/
solClient_rxMsgCallback_returnCode_t
messageReceiveCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_uint32_t *counter_p = user_p;

    ( *counter_p )++;
    /* 
     * Returning SOLCLIENT_CALLBACK_OK will cause the API to free the memory 
     * used by the message.  This is important to avoid leaks in applications
     * that do not intend to manage the release of each message's memory.
     *
     * If the message callback is being used to queue the messages for
     * further processing, it should return SOLCLIENT_CALLBACK_TAKE_MSG. In
     * this case, it becomes the responsibility of the application to free
     * the memory.
     */
    return SOLCLIENT_CALLBACK_OK;
}

/*****************************************************************************
 * flowMessageReceiveCallback
 *
 * The message callback is invoked for each Guaranteed message received by
 * the Flow. The user pointer is expected to be a pointer to a 32-bit 
 * counter that is incremented for each received message.
 * 
 * Message callback code is executed within the API thread, which means that
 * it should deal with the message quickly or spool the message for further
 * processing in another thread.
 * 
 *****************************************************************************/
solClient_rxMsgCallback_returnCode_t
flowMessageReceiveCallback ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_uint32_t *counter_p = user_p;
    solClient_msgId_t msgId;

    ( *counter_p )++;

    /* Acknowledge the message after processing it. */
    if ( solClient_msg_getMsgId ( msg_p, &msgId )  == SOLCLIENT_OK ) {
        printf ( "Acknowledging message Id: %lld.\n", msgId );
        solClient_flow_sendAck ( opaqueFlow_p, msgId );
    }

    /* 
     * Returning SOLCLIENT_CALLBACK_OK causes the API to free the memory 
     * used by the message. This is important to avoid leaks in applications
     * that do not intend to manage the release of each message's memory.
     *
     * If the message callback is being used to queue the messages for
     * further processing, it should return SOLCLIENT_CALLBACK_TAKE_MSG. In
     * this case, it becomes the responsibility of the application to free
     * the memory.
     */
    return SOLCLIENT_CALLBACK_OK;
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

    /* Command Options */
    struct commonOptions commandOpts;

    /* Context */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session */
    solClient_opaqueSession_pt sessionA_p;
    solClient_opaqueSession_pt sessionB_p;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    /* Flow   */
    solClient_opaqueFlow_pt flow_p = NULL;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    solClient_uint32_t msgCounterA = 0; /* counter for received messages */
    solClient_uint32_t msgCounterB = 0; /* counter for received messages */

    /* Session Properties */
    const char     *props[20];
    int             propIndex = 0;

    /* Message */
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;


    printf ( "\nnoLocalPubSub.c (Copyright 2010-2020 Solace Corporation. All rights reserved.)\n" );

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
     * While creating the context, specify that the Context thread be 
     * created automatically instead of having the application create its own
     * Context thread.
     */
    if ( ( rc = solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                           &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create and connect Session 'A'. Session 'A' allows local delivery
     * of Direct messages.
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient session A." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &sessionA_p,
                                                 messageReceiveCallback,
                                                 common_eventCallback, (void *)(&msgCounterA), &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Ensure that the appliance supports No Local delivery
     *************************************************************************/

    /* The same appliance is used for all Sessions, so just check one. */
    if ( !solClient_session_isCapable ( sessionA_p, SOLCLIENT_SESSION_CAPABILITY_NO_LOCAL ) ) {

        solClient_log ( SOLCLIENT_LOG_ERROR, "No Local delivery mode is not supported by peer." );
        goto cleanup;
    }

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient session B." );

    /*************************************************************************
     * Create and connect Session 'B'. Session 'B' disallows local delivery
     * of Direct messages. The common function cannot be used to create this 
     * Session because it has non-standard properties.
     *************************************************************************/


    /* Configure the Session function info. */
    sessionFuncInfo.rxMsgInfo.callback_p = messageReceiveCallback;
    sessionFuncInfo.rxMsgInfo.user_p = (void *)(&msgCounterB);
    sessionFuncInfo.eventInfo.callback_p = common_eventCallback;
    sessionFuncInfo.eventInfo.user_p = NULL;

    /* Configure the Session properties. */
    propIndex = 0;

    if ( commandOpts.targetHost[0] != (char) 0 ) {
        props[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        props[propIndex++] = commandOpts.targetHost;
    }

    props[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    props[propIndex++] = commandOpts.username;

    props[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    props[propIndex++] = commandOpts.password;

    if ( commandOpts.vpn[0] ) {
        props[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
        props[propIndex++] = commandOpts.vpn;
    }

    props[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES;
    props[propIndex++] = "3";


    /*
     * Prevent local delivery by enabling SOLCLIENT_SESSION_PROP_NO_LOCAL property.
     */
    props[propIndex++] = SOLCLIENT_SESSION_PROP_NO_LOCAL;
    props[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    /*
     * The certificate validation property is ignored on non-SSL sessions.
     * For simple demo applications, disable it on SSL sesssions (host
     * string begins with tcps:) so a local trusted root and certificate
     * store is not required. See the  API usres guide for documentation
     * on how to setup a trusted root so the servers certificate returned
     * on the secure connection can be verified if this is desired.
     */
    props[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_VALIDATE_CERTIFICATE;
    props[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;

    props[propIndex] = NULL;

    /* Create the Session. */
    if ( ( rc = solClient_session_create ( props,
                                           context_p,
                                           &sessionB_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto cleanup;
    }

    /* Connect the Session. */
    if ( ( rc = solClient_session_connect ( sessionB_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
        goto cleanup;
    }

    /*************************************************************************
     * Subscribe to the common Topic on sessionB
     *************************************************************************/

    if ( ( rc = solClient_session_topicSubscribeExt ( sessionB_p,
                                                      SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                      COMMON_MY_SAMPLE_TOPIC ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }

    /************************************************************************
     * Provision a Queue on the appliance 
     ***********************************************************************/
    solClient_log ( SOLCLIENT_LOG_INFO, "Creating queue %s on appliance.", COMMON_TESTQ );
    if ( ( rc = common_createQueue ( sessionA_p, COMMON_TESTQ ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createQueue()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Create a Flow to the Queue on sessionA. (Local Delivery is allowed on 
     * the Session but not on the Flow.)
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Bind to Queue %s.", COMMON_TESTQ );
    flowFuncInfo.rxMsgInfo.callback_p = flowMessageReceiveCallback;
    flowFuncInfo.rxMsgInfo.user_p = (void *)(&msgCounterA);       /* NULL or counter pointer */
    flowFuncInfo.eventInfo.callback_p = common_flowEventCallback;
    flowFuncInfo.eventInfo.user_p = NULL;       /* unused in common_flowEventCallback */

    propIndex = 0;

    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    props[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
    props[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    props[propIndex++] = COMMON_TESTQ;

    /* Send an ack when the message has been received.  The default value is
     * to automatically acknowledge on return from the message receive callback
     * but it is recommended to use client acknowledgement when using flows.
     */
    props[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    props[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;

    /*
     * Enable SOLCLIENT_FLOW_PROP_NO_LOCAL property to prevent local delivery on a Queue.
     */
    props[propIndex++] = SOLCLIENT_FLOW_PROP_NO_LOCAL;
    props[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    props[propIndex] = NULL;

    if ( ( rc = solClient_session_createFlow ( props,
                                               sessionA_p, &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_createFlow() did not return SOLCLIENT_OK" );
        goto sessionConnected;
    }


    /*************************************************************************
     * Publish a message on sessionA that will be received on SessionB.
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Publishing messages.\n" );

    /* Allocate memory for the message to be sent. */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        goto sessionConnected;
    }

    /* Set the message delivery mode. */
    if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_DIRECT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto freeMessage;
    }

    /* Set the destination. */
    destination.destType = SOLCLIENT_TOPIC_DESTINATION;
    destination.dest = COMMON_MY_SAMPLE_TOPIC;
    if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMessage;
    }

    /* Add some content to the message. */
    if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p,
                                                    COMMON_ATTACHMENT_TEXT,
							(solClient_uint32_t)(strlen ( COMMON_ATTACHMENT_TEXT ) ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setBinaryAttachment()" );
        goto freeMessage;
    }

    /* Send the message. */
    if ( ( rc = solClient_session_sendMsg ( sessionA_p, msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_sendMsg()" );
        goto freeMessage;
    }
    /* Pause to let the callback receive messages. */
    sleepInSec ( 1 );

    /* Should be seen only on Session B. */
    if ( msgCounterA != 0 || msgCounterB != 1 ) {
        common_handleError ( rc, "Published direct message seen on session A or not seen on session B" );
        goto freeMessage;
    }
    /* Reset msgCounterB. */
    msgCounterB = 0;

    /*************************************************************************
     * Publish a message on SessionB that will be not be received at all.
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Publishing message on Session B.\n" );

    /* Send the message. */
    if ( ( rc = solClient_session_sendMsg ( sessionB_p, msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_sendMsg()" );
        goto freeMessage;
    }
    /* Pause to let callback receive messages. */
    sleepInSec ( 1 );

    /* Should not be seen. */
    if ( msgCounterA != 0 || msgCounterB != 0 ) {
        common_handleError ( rc, "Published direct message seen on session A or on session B" );
        goto freeMessage;
    }

    /*************************************************************************
     * Publish a message on SessionA to COMMON_TESTQ that will be not be
     * received at all. NOTE: It is expected that the appliance will reject this 
     * message because it is published to the Queue name and cannot be accepted.
     * The test should report a "No Local Discard" message rejection received
     * from the appliance.
     *************************************************************************/

    printf ( "\nnoLocalPubSub: Publishing a message that will be rejected by appliance due to No Local Discard\n\nWaiting for Event ... \n\n" );

    /* Set the message delivery mode. */
    if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto freeMessage;
    }

    /* Set the destination. */
    destination.destType = SOLCLIENT_QUEUE_DESTINATION;
    destination.dest = COMMON_TESTQ;
    if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMessage;
    }

    /* Send the message. */
    if ( ( rc = solClient_session_sendMsg ( sessionA_p, msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_sendMsg()" );
        goto freeMessage;
    }
    /* Pause to let callback receive messages. */
    sleepInSec ( 1 );

    /* The message should not be seen. */
    if ( msgCounterA != 0 || msgCounterB != 0 ) {
        common_handleError ( rc, "Published persistent message seen on session A or on session B" );
        goto freeMessage;
    }

    /*************************************************************************
     * Publish a message to a Queue on SessionB; the message will be be received
     * on the Flow on sessionA.
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Publishing message on Session B.\n" );

    /* Send the message. */
    if ( ( rc = solClient_session_sendMsg ( sessionB_p, msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_sendMsg()" );
        goto freeMessage;
    }
    /* Pause to let callback receive messages. */
    sleepInSec ( 1 );

    if ( msgCounterA != 1 || msgCounterB != 0 ) {
        common_handleError ( rc, "Published persistent message not seen on session A or seen on session B" );
        goto freeMessage;
    }

    printf ( "\nTest Passed\n" );

  freeMessage:
    if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
        goto sessionConnected;
    }


    /*************************************************************************
     * Unsubscribe
     *************************************************************************/

    if ( ( rc = solClient_session_topicUnsubscribeExt ( sessionB_p,
                                                        SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                        COMMON_MY_SAMPLE_TOPIC ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }



    /*************************************************************************
     * CLEANUP
     *************************************************************************/
  sessionConnected:
    if ( flow_p != NULL ) {
        /*
         * Destroy the Flow before deleting the Queue or else the API will log at
         * NOTICE level for the unsolicited unbind.
         */
        if ( ( rc = solClient_flow_destroy ( &flow_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_flow_destroy()" );
        }
    }

    /************************************************************************
     * REMOVE QUEUE ON THE APPLIANCE 
     ***********************************************************************/
    if ( ( rc = common_deleteQueue ( sessionA_p, COMMON_TESTQ ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_deleteQueue()" );
    }

    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( sessionA_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }
    if ( ( rc = solClient_session_disconnect ( sessionB_p ) ) != SOLCLIENT_OK ) {
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
