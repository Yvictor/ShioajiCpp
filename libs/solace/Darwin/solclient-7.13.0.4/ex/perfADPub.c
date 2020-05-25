
/** @example ex/perfADPub.c 
 */

/*
 * This sample shows a Guaranteed Messaging throughput publishing example for the 
 * Solace Messaging API for C. 
 *
 * Copyright 2007-2020 Solace Corporation. All rights reserved.
 */

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"

/*
 * fn rxPerfCallbackFunc()
 * A solClient_session_rxCallbackFunc_t that does nothing when called.
 * To be used as part of a solClient_session_createFuncInfo_t
 * passed to a solClient_session_create().
 */
static          solClient_rxMsgCallback_returnCode_t
rxPerfMsgCallbackFunc ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    /* Do nothing as this sample is only a publisher. */
    return SOLCLIENT_CALLBACK_OK;
}

/*
 * fn main() 
 * param appliance_ip address The message backbone IP address.
 * param appliance_username The client username for a Solace appliance running SolOS-TR.
 * param dest_name The Topic or Queue to publish to.
 * param pub_mode Publish mode. One of:
 *       persistentT: For publishing persistent messages to a Topic.
 *       non-persistentT: For publishing non-persistent messages to a Queue.
 *       persistentQ: For publishing persistent messages to a Queue.
 *       non-persistentQ: For publishing non-persistent messages to a Queue.
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
    int             propIndex;

    int             loop;
    char            binMsg[1024];
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;
    solClient_uint32_t deliveryMode;
    char            gdWindowStr[32];

    long long       startTime;
    long long       targetTime;
    long long       currentTime;
    long long       timeDiff;
    long long       elapsedTime;
    long double     usPerMsg;
    char            positionalParms[] = "\t[mode]          Delivery Mode (persistentQ, non-persistentQ, persistentT,\n"\
"\t                    non-persistentT - default persistentQ.)\n";

    printf ( "\nperfADPub.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n" );

    /*************************************************************************
     * PARSE COMMAND OPTIONS
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK |
                                DEST_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK |
                                PASS_PARAM_MASK |
                                NUM_MSGS_MASK  |
                                MSG_RATE_MASK |
                                WINDOW_SIZE_MASK |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, positionalParms ) == 0 ) {
        exit(1);
    }

    /*
     * Set the destination and deliveryMode parameters, default to queue
     */
    destination.dest = commandOpts.destinationName;
    /*
     * If the user specified the deliveryMode string, override the 
     * default 
     */
    if (optind < argc ) {
        if ( strcmp ( "persistentQ", argv[optind] ) == 0 ) {
            deliveryMode = SOLCLIENT_DELIVERY_MODE_PERSISTENT;
            destination.destType = SOLCLIENT_QUEUE_DESTINATION;
        } else if ( strcmp ( "non-persistentQ", argv[optind] ) == 0 ) {
            deliveryMode = SOLCLIENT_DELIVERY_MODE_NONPERSISTENT;
            destination.destType = SOLCLIENT_QUEUE_DESTINATION;
        } else if ( strcmp ( "persistentT", argv[optind] ) == 0 ) {
            deliveryMode = SOLCLIENT_DELIVERY_MODE_PERSISTENT;
            destination.destType = SOLCLIENT_TOPIC_DESTINATION;
        } else if ( strcmp ( "non-persistentT", argv[optind] ) == 0 ) {
            deliveryMode = SOLCLIENT_DELIVERY_MODE_NONPERSISTENT;
            destination.destType = SOLCLIENT_TOPIC_DESTINATION;
        } else {
            printf ( "Invalid mode parameter '%s' - must be one of 'persistentQ', 'non-persistentQ', 'persistentT', 'non-persistentT'\n",
                argv[optind]);
            exit(1);
        }
    } else {
        deliveryMode = SOLCLIENT_DELIVERY_MODE_PERSISTENT;
        destination.destType = SOLCLIENT_QUEUE_DESTINATION;
    }

    /*************************************************************************
     * Initialize THE API and setup logging level
     *************************************************************************/

    /* solClient needs to be initialized before any other API calls are made. */
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

    sessionFuncInfo.rxMsgInfo.callback_p = rxPerfMsgCallbackFunc;
    sessionFuncInfo.eventInfo.callback_p = common_eventPerfCallback;

    /* Create a Session for sending and receiving messages. */
    propIndex = 0;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = commandOpts.username;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = commandOpts.password;

    if ( commandOpts.targetHost[0] != (char) 0 ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = commandOpts.targetHost;
    }
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SEND_BLOCKING;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL;
    sessionProps[propIndex++] = ( commandOpts.enableCompression ) ? "9" : "0";

    /* If publish window size was specified on the command line, then set it here
     * in the Session properties. */
    if ( commandOpts.gdWindow != 0 ) {
        snprintf ( gdWindowStr, sizeof ( gdWindowStr ), "%d", commandOpts.gdWindow );
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PUB_WINDOW_SIZE;
        sessionProps[propIndex++] = gdWindowStr;
    }
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

    if ( commandOpts.useGSS ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME;
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME_GSS_KRB;
    }

    sessionProps[propIndex++] = NULL;

    solClient_log ( SOLCLIENT_LOG_INFO, "creating solClient session" );
    if ( ( rc = solClient_session_create ( sessionProps,
                                           context_p, &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) )
         != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto cleanup;
    }

    /* Connect the Session. */
    solClient_log ( SOLCLIENT_LOG_INFO, "connecting solClient session" );
    if ( ( rc = solClient_session_connect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
        goto cleanup;
    }

    /*************************************************************************
     * Publish
     *************************************************************************/

    /* 
     * Prepare the message to send. Use the same message for each send
     * operation.
     */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        goto sessionConnected;
    }
    /* Set the delivery mode for the message. */
    if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, deliveryMode ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
        goto sessionConnected;
    }
    /* Initialize a binary attachment, and use it as part of the message */
    memset ( ( void * ) binMsg, 0xab, sizeof ( binMsg ) );
    if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p, binMsg, sizeof ( binMsg ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setBinaryAttachmentPtr()" );
        goto sessionConnected;
    }

    if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto sessionConnected;
    }

    /* Send a group of messages. */
    usPerMsg = ( long double ) 1000000.0 / ( long double ) commandOpts.msgRate;

    startTime = getTimeInUs (  );
    targetTime = startTime + ( long long ) usPerMsg;

    for ( loop = 0; loop < commandOpts.numMsgsToSend; loop++ ) {
        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_send" );
            break;
        }
        /* Check message rate every message. */
        currentTime = getTimeInUs (  );
        timeDiff = targetTime - currentTime;
        if ( timeDiff > 0 ) {
            sleepInUs ( ( int ) ( timeDiff ) );
        }
        targetTime += ( long long ) usPerMsg;

    }

    elapsedTime = getTimeInUs (  ) - startTime;
    printf ( "Sent %d msgs in %lld usec, rate of %Lf msgs/sec\n",
             commandOpts.numMsgsToSend, elapsedTime,
             ( long double ) commandOpts.numMsgsToSend / ( ( long double ) elapsedTime / ( long double ) 1000000.0 ) );

    solClient_msg_free ( &msg_p );

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
    goto notInitialized;

  notInitialized:
    /* Nothing to do - just exit. */

    return 0;
}                               //End main()
