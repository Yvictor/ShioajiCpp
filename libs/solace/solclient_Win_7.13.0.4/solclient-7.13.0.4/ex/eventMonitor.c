
/** @example ex/eventMonitor.c 
 */

//
// This sample demonstrates:
//  - Monitoring appliance events using a relevant appliance event subscription.
//
// Sample Requirements:
//  - A Solace appliance running SolOS-TR.
//  - The CLI configuration "Publish Client Event Messages" must be enabled 
//    in the client's Message VPN on the appliance.
//
// This sample, a subscription is made to the appliance Event Topic for Client Connect
// events:
//
//       #LOG/INFO/CLIENT/<appliance hostname>/CLIENT_CLIENT_CONNECT/>
//
// With "Publish Client Event Messages" enabled for the Message VPN,
// all client events are published as messages. By subscribing to the above Topic,
// all CLIENT_CLIENT_CONNECT event messages are received from the specified
// appliance.
//
// Event message topics are treated as regular Topics in that wildcarding can be
// used in the same manner as typical Topics. For example, if you want to
// receive all client events, regardless of Event Level, the following Topic
// could be used:
//     #LOG/*/CLIENT/<appliance hostname>/>
//

/*
 * This sample triggers a CLIENT_CLIENT_CONNECT event by connecting a second
 * time to the router (triggerSecondaryConnection()).
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
 * messageReceiveEventMonitorCallback
 *****************************************************************************/
solClient_rxMsgCallback_returnCode_t
messageReceiveEventMonitorCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;
    solClient_destination_t destination;

    void           *binaryAttachmentBuffer_p;
    solClient_uint32_t binaryAttachmentBufferSize;


    if ( ( rc = solClient_msg_getDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_getDestination()" );
        return SOLCLIENT_CALLBACK_OK;
    }

    if ( ( rc = solClient_msg_getBinaryAttachmentPtr ( msg_p,
                                                       &binaryAttachmentBuffer_p,
                                                       &binaryAttachmentBufferSize ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_getBinaryAttachmentPtr()" );
        return SOLCLIENT_CALLBACK_OK;
    }

    printf ( "*** Event Message Received ***\n" );
    printf ( "Topic:\t%s\n", destination.dest );
    printf ( "Event:\t%s\n", ( char * ) binaryAttachmentBuffer_p );

    /* 
     * Returning SOLCLIENT_CALLBACK_OK causes the API to free the memory 
     * used by the message. This is important to avoid leaks.
     */
    return SOLCLIENT_CALLBACK_OK;

}

/*****************************************************************************
 * triggerSecondaryConnection
 *****************************************************************************/
solClient_returnCode_t
triggerSecondaryConnection ( solClient_opaqueContext_pt context_p, struct commonOptions * commandOpts )
{
    solClient_opaqueSession_pt session_p;
    solClient_returnCode_t rc = SOLCLIENT_OK;

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback, NULL, commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        return rc;
    }

    if ( ( rc = solClient_session_topicSubscribeExt ( session_p,
                                                      SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                                      COMMON_MY_SAMPLE_TOPIC ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
    }

    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

    return rc;
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

    /*
     * When Event Message Publishing is enabled for a Message VPN, clients can
     * subscribe to a specific Topic to receive the event messages.
     *
     * Format for Event topics: 
     *      #LOG/<level>/<type>/<appliance hostname>/<event name>/...
     */
    char           *eventTopicFormat = "#LOG/INFO/CLIENT/%s/CLIENT_CLIENT_CONNECT/>";

    char            eventTopic[SOLCLIENT_BUFINFO_MAX_TOPIC_SIZE + 1];
    solClient_field_t routerName;


    printf ( "\neventMonitor.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control-C handling. */
    initSigHandler (  );

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
        exit (1);
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
     * CREATE A CONTEXT
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

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient sessions." );

    /* 
     * createAndConnectSession is a common function used in these samples.
     * It is a wrapper for solClient_session_create() that applies some 
     * common properties to the session, some of which are based on the 
     * command options. The wrapper also connects the Session.
     */
    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 messageReceiveEventMonitorCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * SUBSCRIBE
     *************************************************************************/

    /* 
     * Get the hostname of the appliance. The hostname is part of the Topic
     * used to subscribe to Event Messages.
     */
    if ( ( rc = solClient_session_getCapability ( session_p,
                                                  SOLCLIENT_SESSION_PEER_ROUTER_NAME,
                                                  &routerName, sizeof ( routerName ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_getCapability()" );
        goto sessionConnected;
    }

    snprintf ( eventTopic, sizeof ( eventTopic ), eventTopicFormat, routerName.value.string );

    if ( ( rc = solClient_session_topicSubscribeExt ( session_p,
                                                      SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM, eventTopic ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_topicSubscribe()" );
        goto sessionConnected;
    }

    if ( ( rc = triggerSecondaryConnection ( context_p, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "triggerSecondaryConnection()" );
        goto sessionConnected;
    }

    /* Sleep to allow reception of event. */
    sleepInSec ( 1 );
    printf ( "Cleaning up.\n" );

    /*************************************************************************
     * CLEANUP
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
