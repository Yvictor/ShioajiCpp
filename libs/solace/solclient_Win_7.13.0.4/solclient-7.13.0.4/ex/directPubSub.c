
/** @example ex/directPubSub.c 
 */

/*
 * This sample demonstrates:
 *  - Subscribing to a Topic for Direct messages.
 *  - Publishing Direct messages to a Topic.
 *  - Receiving messages through a callback function.
 *
 * This sample shows the basics of creating a Context, creating a
 * Session, connecting a Session, subscribing to a Topic, and publishing 
 * Direct messages to a Topic. This is meant to be a very basic example, so 
 * there are minimal Session properties, and it uses a message callback that 
 * simply prints any received message to the screen.
 *
 * Although other samples make use of common code to perform some of the
 * most common actions, this sample explicitly includes many of these common
 * functions in this sample to emphasize the most basic building blocks of
 * any application.
 *
 * Copyright 2009-2020 Solace Corporation. All rights reserved.
 */

/*****************************************************************************
    For Windows builds, os.h should always be included first to ensure that
    _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *****************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"

#ifndef LLONG_MAX
    #ifndef INT64_MAX
        #define INT64_MAX (9223372036854775807LL)
    #endif
#define LLONG_MAX INT64_MAX
#endif

solClient_int64_t minTransitTime_s = LLONG_MAX, maxTransitTime_s = 0, totalTransitTime_s = 0;

/*****************************************************************************
 * messageReceiveCallback
 *
 * The message callback is invoked for each Direct message received by
 * the Session. In this sample, the message is printed to the screen.
 * 
 * Message callback code is executed within the API thread, which means that
 * it should deal with the message quickly, or queue the message for further
 * processing in another thread.
 * 
 * Note: In other samples, a common message handler is used. However, to
 * emphasize this programming paradigm, this sample directly includes the message
 * receive callback.
 *****************************************************************************/
solClient_rxMsgCallback_returnCode_t
messageReceiveCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;
    solClient_int64_t sendTimestamp, rcvTimestamp, transitTime;

    printf ( "Received message:\n" );
    if ( ( rc = solClient_msg_dump ( msg_p, NULL, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_dump()" );
        return SOLCLIENT_CALLBACK_OK;
    }
    if ( SOLCLIENT_OK != solClient_msg_getSenderTimestamp(msg_p, &sendTimestamp) ) {
        common_handleError ( rc, "solClient_msg_getSenderTimestamp()" );
        return SOLCLIENT_CALLBACK_OK;
    }
    if ( SOLCLIENT_OK != solClient_msg_getRcvTimestamp(msg_p, &rcvTimestamp) ) {
        common_handleError ( rc, "solClient_msg_getRcvTimestamp()" );
        return SOLCLIENT_CALLBACK_OK;
    }
    transitTime = rcvTimestamp - sendTimestamp;

    if ( transitTime < minTransitTime_s ) minTransitTime_s = transitTime;
    if ( transitTime > maxTransitTime_s ) maxTransitTime_s = transitTime;
    totalTransitTime_s += transitTime;


    printf ( "\n" );

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
    solClient_opaqueSession_pt session_p;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    /* Session Properties */
    const char     *sessionProps[50];
    int             propIndex = 0;

    /* Message */
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;
    int             msgsSent = 0;


    printf ( "\ndirectPubSub.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK |
                                DEST_PARAM_MASK | 
                                PASS_PARAM_MASK |
                                NUM_MSGS_MASK  |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, NULL ) == 0 ) {
        exit (1);
    }

    /* use a default topic is a topic is not specified */
    if ( commandOpts.destinationName[0] == ( char ) 0 ) {
        strncpy (commandOpts.destinationName, COMMON_MY_SAMPLE_TOPIC, sizeof(commandOpts.destinationName));
    }
    /*************************************************************************
     * Initialize the API (and setup logging level)
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

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient session." );

    /*
     * Note: In other samples, common functions have been used to create 
     * and connect Sessions. However, for demonstration purposes, this sample
     * includes Session creation and connection in line.
     */

    /* Configure the Session function information. */
    sessionFuncInfo.rxMsgInfo.callback_p = messageReceiveCallback;
    sessionFuncInfo.rxMsgInfo.user_p = NULL;
    sessionFuncInfo.eventInfo.callback_p = common_eventCallback;
    sessionFuncInfo.eventInfo.user_p = NULL;

    /* Configure the Session properties. */
    propIndex = 0;

    if ( commandOpts.targetHost[0] != (char) 0 ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = commandOpts.targetHost;
    }

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = commandOpts.username;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = commandOpts.password;

    if ( commandOpts.vpn[0] ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
        sessionProps[propIndex++] = commandOpts.vpn;
    }

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES;
    sessionProps[propIndex++] = "3";

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL;
    sessionProps[propIndex++] = ( commandOpts.enableCompression ) ? "9" : "0";

    /*
     * Note: Reapplying subscriptions allows Sessions to reconnect after
     * failure and have all their subscriptions automatically restored. For
     * Sessions with many subscriptions this can increase the amount of time
     * required for a successful reconnect.
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    /*
     * The certificate validation property is ignored on non-SSL sessions.
     * For simple demo applications, disable it on SSL sessions (host
     * string begins with tcps:) so a local trusted root and certificate
     * store is not required. See the API Developer Guide for documentation
     * on how to setup a trusted root so the server certificate returned
     * on the secure connection can be verified if this is desired.
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_VALIDATE_CERTIFICATE;
    sessionProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;

    if ( commandOpts.useGSS ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME;
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME_GSS_KRB;
    }
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_SEND_TIMESTAMPS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_GENERATE_RCV_TIMESTAMPS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    sessionProps[propIndex] = NULL;

    /* Create the Session. */
    if ( ( rc = solClient_session_create ( sessionProps,
                                           context_p,
                                           &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto cleanup;
    }

    /* Connect the Session. */
    if ( ( rc = solClient_session_connect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
        goto cleanup;
    }

    /*************************************************************************
     * Subscribe
     *************************************************************************/

    if ( ( rc = solClient_session_topicSubscribeExt ( session_p,
                                                      SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                      commandOpts.destinationName ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Publish
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Publishing messages.\n" );

    for ( msgsSent = 0; msgsSent < commandOpts.numMsgsToSend; ++msgsSent ) {

        /* Allocate memory for the message that is to be sent. */
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
        destination.dest = commandOpts.destinationName;
        if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDestination()" );
            goto freeMessage;
        }

        /* Add some content to the message. */
        if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p,
                                                        COMMON_ATTACHMENT_TEXT,
                                                        ( solClient_uint32_t ) strlen ( COMMON_ATTACHMENT_TEXT ) ) ) !=
             SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setBinaryAttachment()" );
            goto freeMessage;
        }

        /* Send the message. */
        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_sendMsg()" );
            goto freeMessage;
        }

      freeMessage:
        if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_free()" );
            goto sessionConnected;
        }

        /* 
         * Wait one second between sending messages. This provides time for 
         * the final message to be received.
         */
        sleepInSec ( 1 );
    }

    /*************************************************************************
     * Unsubscribe
     *************************************************************************/

    if ( ( rc = solClient_session_topicUnsubscribeExt ( session_p,
                                                        SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                        commandOpts.destinationName ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }

    printf ("Summary:\n");
    printf ("   Maximum Transit Time = %lld\n", maxTransitTime_s);
    printf ("   Minimum Transit Time = %lld\n", minTransitTime_s);
    printf ("   Average Transit Time = %lld\n", totalTransitTime_s / commandOpts.numMsgsToSend);

    /*************************************************************************
     * Cleanup
     *************************************************************************/
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

}
