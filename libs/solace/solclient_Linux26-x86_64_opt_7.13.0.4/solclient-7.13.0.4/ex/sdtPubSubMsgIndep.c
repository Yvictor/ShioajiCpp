
/** @example ex/sdtPubSubMsgIndep.c 
 */

/*
 * This sample demonstrates:
 *  - Subscribing to a Topic.
 *  - Publishing SDT Map messages to the Topic.
 *
 * This sample creates a SDT Stream and a SDT Map in local memory (independent
 * of the Solace Msg). The Stream is added to the binary attachment of a
 * message. The Map is made the user property map of the message.
 *
 * The Map is reused and modified for multiple sends.
 *
 * Message-independent Streams and Map are useful when you want 
 * to reuse them in multiple messages. When using message-independent 
 * Streams and Maps, the client application is responsible for 
 * managing the memory that is allocated.
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

    /* Message */
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;
    int             msgsSent = 0;
    solClient_opaqueContainer_pt streamContainer = NULL;
    solClient_opaqueContainer_pt userPropContainer = NULL;
    char            stream[1024];
    char            map[1024];
    char            messageField[32];

    printf ( "\nsdtPubSubMsgIndep.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK | 
                                PASS_PARAM_MASK |
                                NUM_MSGS_MASK  |
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
                                           &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto cleanup;
    }

    /*************************************************************************
     * Create and connect a Session
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient session." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Subscribe
     *************************************************************************/

    if ( ( rc = solClient_session_topicSubscribeExt ( session_p,
                                                      SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                      COMMON_MY_SAMPLE_TOPIC ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Publish
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Publishing messages.\n" );

    /* Create the message-independent Stream. */
    if ( ( rc = solClient_container_createStream ( &streamContainer, stream, sizeof ( stream ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_createStream()" );
        goto sessionConnected;
    }

    /* Add PI to the Stream. */
    if ( ( rc = solClient_container_addDouble ( streamContainer, 3.141592654, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_addDouble()" );
        goto freeContainer;
    }

    /* Add a String to the Stream. */
    if ( ( rc = solClient_container_addString ( streamContainer, "message", NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_addString()" );
        goto freeContainer;
    }

    /* Create the message independent Map. */
    if ( ( rc = solClient_container_createMap ( &userPropContainer, map, sizeof ( map ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_createMap()" );
        goto sessionConnected;
    }

    /* Add the largest known Mersenne Exponent to the Map. */
    if ( ( rc = solClient_container_addInt32 ( userPropContainer, 43112609, "mersenne" ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_container_addInt32()" );
        goto freeContainer;
    }

    /* Allocate memory for the message to be sent. */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        goto freeContainer;
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

    for ( msgsSent = 0; msgsSent < 10; ++msgsSent ) {
        if ( ( rc = solClient_container_deleteField ( userPropContainer, "message" ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_container_deleteField()" );
            goto freeMessage;
        }

        snprintf ( messageField, 32, "message%d", ( msgsSent + 1 ) );

        /* Populate the Map with a string. */
        if ( ( rc = solClient_container_addString ( userPropContainer, messageField, "message" ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_container_addString()" );
            goto freeMessage;
        }

        /* Set the binary attachment that is to be the Stream. */
        if ( ( rc = solClient_msg_setBinaryAttachmentContainer ( msg_p, streamContainer ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setBinaryAttachmentContainer()" );
            goto freeMessage;
        }

        /* Set the user property map attachment that is to be the Map. */
        if ( ( rc = solClient_msg_setUserPropertyMap ( msg_p, userPropContainer ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setUserPropertyMap()" );
            goto freeMessage;
        }

        /* Send the message. */
        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_sendMsg()" );
            goto freeMessage;
        }
    }

  freeMessage:
    if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
        goto sessionConnected;
    }

  freeContainer:
    solClient_container_closeMapStream ( &userPropContainer );
    solClient_container_closeMapStream ( &streamContainer );

    /* 
     * Wait one second after sending messages. This will also give time 
     * for the final message to be received.
     */
    sleepInSec ( 1 );

    /*************************************************************************
     * Unsubscribe
     *************************************************************************/

    if ( ( rc = solClient_session_topicUnsubscribeExt ( session_p,
                                                        SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                        COMMON_MY_SAMPLE_TOPIC ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }

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
