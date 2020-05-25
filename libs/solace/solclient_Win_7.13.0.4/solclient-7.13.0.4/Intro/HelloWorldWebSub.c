/** @example ex/Intro/HelloWorldWebSub.c
 */
/*
 *  Copyright 2012-2020 Solace Corporation. All rights reserved.
 *
 *  http://www.solace.com
 *
 *  This source is distributed under the terms and conditions
 *  of any contract or contracts between Solace and you or
 *  your company. If there are no contracts in place use of
 *  this source is not authorized. No support is provided and
 *  no distribution, sharing with others or re-use of this
 *  source is authorized unless specifically stated in the
 *  contracts referred to above.
 *
 *  HelloWorldWebSub
 *
 *  This sample shows the basics of creating a web messaging session, connecting a web 
 *  messaging session, and receiving a direct message from a topic. This is meant to be
 *  a very basic example for demonstration purposes.
 */

#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"


/* Message Count */
static int msgCount = 0;

/*****************************************************************************
 * messageReceiveCallback
 *
 * The message callback is invoked for each Direct message received by
 * the Session. In this sample, the message is printed to the screen.
 *****************************************************************************/
solClient_rxMsgCallback_returnCode_t
messageReceiveCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    printf ( "Received message:\n" );
    solClient_msg_dump ( msg_p, NULL, 0 );
    printf ( "\n" );

    msgCount++;

    return SOLCLIENT_CALLBACK_OK;
}

/*****************************************************************************
 * eventCallback
 *
 * The event callback function is mandatory for session creation.
 *****************************************************************************/
void
eventCallback ( solClient_opaqueSession_pt opaqueSession_p,
                solClient_session_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
}

/*****************************************************************************
 * main
 * 
 * The entry point to the application.
 *****************************************************************************/
int
main ( int argc, char *argv[] )
{
    /* Context */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session */
    solClient_opaqueSession_pt session_p;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    /* Session Properties */
    const char     *sessionProps[20];
    int             propIndex = 0;

    /* API return code */
    solClient_returnCode_t      rc;

    if ( argc < 5 ) {
        printf ( "Usage: HelloWorldWebSub <http://msg_backbone_ip[:port]> <vpn> <client-username> <topic> [web-transport-protocol]\n");
        return -1;
    }

    if ((strncmp(argv[1], "http", 4) != 0) && (strncmp(argv[1], "ws", 2) != 0)){
        printf ("%s: support HTTP or WS transport protocols only\n", argv[1]);
        printf ( "\t Usage: HelloWorldWebSub <http://msg_backbone_ip[:port]> <vpn> <client-username> <topic> [web-transport-protocol]\n");
        return -1;
    }

    /*************************************************************************
     * Initialize the API (and setup logging level)
     *************************************************************************/

    /* solClient needs to be initialized before any other API calls. */
    rc = solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL );
    if (rc != SOLCLIENT_OK) {
        printf ( "solClient_initialize: returnCode  %d (expect %d)\n", rc,  SOLCLIENT_OK);
        return -1;
    }

    printf ( "HelloWorldWebSub initializing...\n" );

    /*************************************************************************
     * Create a Context
     *************************************************************************/

    /* 
     * Create a Context, and specify that the Context thread be created 
     * automatically instead of having the application create its own
     * Context thread.
     */
    rc = solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                   &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) );

    if (rc != SOLCLIENT_OK) {
        printf ( "solClient_context_create: returnCode  %d (expect %d)\n", rc,  SOLCLIENT_OK);
        return -1;
    }

    /*************************************************************************
     * Create and connect a Session
     *************************************************************************/

    /* Configure the Session function information. */
    sessionFuncInfo.rxMsgInfo.callback_p = messageReceiveCallback;
    sessionFuncInfo.rxMsgInfo.user_p = NULL;
    sessionFuncInfo.eventInfo.callback_p = eventCallback;
    sessionFuncInfo.eventInfo.user_p = NULL;

    /* Configure the Session properties. */
    propIndex = 0;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
    sessionProps[propIndex++] = argv[1];

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
    sessionProps[propIndex++] = argv[2];

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = argv[3];
 
    if (argc > 5) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_WEB_TRANSPORT_PROTOCOL_LIST;
        sessionProps[propIndex++] = argv[5];
    }

    sessionProps[propIndex] = NULL;

    /* Create the Session. */
    rc = solClient_session_create ( ( char ** ) sessionProps,
                                    context_p,
                                    &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) );

    if (rc != SOLCLIENT_OK) {
        printf ( "solClient_session_create: returnCode  %d (expect %d)\n", rc,  SOLCLIENT_OK);
        return -1;
    }

    /* Connect the Session. */
    rc =  solClient_session_connect ( session_p );
    if (rc != SOLCLIENT_OK) {
        printf ( "solClient_session_connect: returnCode  %d (expect %d)\n", rc,  SOLCLIENT_OK);
        return -1;
    }

    printf ( "Connected.\n" );

    /*************************************************************************
     * Subscribe
     *************************************************************************/

    rc = solClient_session_topicSubscribeExt ( session_p,
                                          SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                          argv[4] );

    if (rc != SOLCLIENT_OK) {
        printf ( "solClient_session_topicSubscribeExt: returnCode  %d (expect %d)\n", rc,  SOLCLIENT_OK);
        return -1;
    }

    /*************************************************************************
     * Wait for message
     *************************************************************************/

    printf ( "Waiting for message......\n" );
    fflush ( stdout );
    while ( msgCount < 1 ) {
        SLEEP ( 1 );
    }

    printf ( "Exiting.\n" );

    /*************************************************************************
     * Unsubscribe
     *************************************************************************/

    rc = solClient_session_topicUnsubscribeExt ( session_p,
                                            SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                                            argv[4] );
    if (rc != SOLCLIENT_OK) {
        printf ( "solClient_session_topicUnsubscribeExt: returnCode  %d (expect %d)\n", rc,  SOLCLIENT_OK);
        return -1;
    }

    /*************************************************************************
     * Cleanup
     *************************************************************************/

    /* Cleanup solClient. */
    solClient_cleanup (  );

    return 0;
}
