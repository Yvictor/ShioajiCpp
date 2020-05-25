
/** @example ex/replication.c 
 */

/*
 * This sample shows the publishing of Guaranteed messages through 
 * a host list reconnect
 *
 * In the event callback, the publisher recognizes and displays
 * the content of events that may be seen when the session to
 * the original message-router fails and reconnects to the next
 * message-router in the host list.
 *
 * The easiest way to force a reconnect from one host to the next is
 * to shutdown the client-username in the first message-router after
 * connecting with this application.
 *
 * Copyright 2010-2020 Solace Corporation. All rights reserved.
 */

/**************************************************************************
    For Windows builds, os.h should always be included first to ensure that
    _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"
#include "common.h"


/**
 * @struct publisherInfoStruct
 *
 * A structure that is to track statistics for the publisher.
 */
typedef struct publisherInfoStruct
{
    int         acknowledgementRx_m;     /* Number of acknowledgements seen by publisher */
    int         rejectedMsgRx_m;         /* Number of rejected message errors seen by publisher */
} publisherInfoStruct_t, *publisherInfoStruct_pt; 

void
replication_eventCallback ( solClient_opaqueSession_pt opaqueSession_p,
                         solClient_session_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
    solClient_errorInfo_pt errorInfo_p;
    publisherInfoStruct_pt      pubInfo_p = ( publisherInfoStruct_pt ) user_p;

    switch ( eventInfo_p->sessionEvent ) {
        case SOLCLIENT_SESSION_EVENT_ACKNOWLEDGEMENT:
            /* Non-error events are logged at the INFO level. */
            solClient_log ( SOLCLIENT_LOG_INFO,
                            "replication_eventCallback() called - %s\n",
                            solClient_session_eventToString ( eventInfo_p->sessionEvent ));

            pubInfo_p->acknowledgementRx_m ++;
            break;

        case SOLCLIENT_SESSION_EVENT_REJECTED_MSG_ERROR:
            /* Extra error information is available on error events */
            errorInfo_p = solClient_getLastErrorInfo (  );
            /* Error events are output to STDOUT. */
            printf ( "replication_eventCallback() called - %s; subCode %s, responseCode %d, reason %s\n",
                     solClient_session_eventToString ( eventInfo_p->sessionEvent ),
                     solClient_subCodeToString ( errorInfo_p->subCode ), errorInfo_p->responseCode, errorInfo_p->errorStr );


            pubInfo_p->rejectedMsgRx_m ++;
            break;

        case SOLCLIENT_SESSION_EVENT_UP_NOTICE:
        case SOLCLIENT_SESSION_EVENT_TE_UNSUBSCRIBE_OK:
        case SOLCLIENT_SESSION_EVENT_CAN_SEND:
        case SOLCLIENT_SESSION_EVENT_RECONNECTING_NOTICE:
        case SOLCLIENT_SESSION_EVENT_RECONNECTED_NOTICE:
        case SOLCLIENT_SESSION_EVENT_PROVISION_OK:
        case SOLCLIENT_SESSION_EVENT_SUBSCRIPTION_OK:

            /* Non-error events are logged at the INFO level. */
            solClient_log ( SOLCLIENT_LOG_INFO,
                            "replication_eventCallback() called - %s\n",
                            solClient_session_eventToString ( eventInfo_p->sessionEvent ));
            break;

        case SOLCLIENT_SESSION_EVENT_VIRTUAL_ROUTER_NAME_CHANGED:
        case SOLCLIENT_SESSION_EVENT_REPUBLISH_UNACKED_MESSAGES:
            /*
             * Information events that we expect in this sample.  Output information to STDOUT.
             */
            printf ( "replication_eventCallback() called - %s; info_p %s\n",
                      solClient_session_eventToString ( eventInfo_p->sessionEvent ),
                      eventInfo_p->info_p );
            break;


        case SOLCLIENT_SESSION_EVENT_DOWN_ERROR:
        case SOLCLIENT_SESSION_EVENT_CONNECT_FAILED_ERROR:
        case SOLCLIENT_SESSION_EVENT_SUBSCRIPTION_ERROR:
        case SOLCLIENT_SESSION_EVENT_TE_UNSUBSCRIBE_ERROR:
        case SOLCLIENT_SESSION_EVENT_PROVISION_ERROR:

            /* Extra error information is available on error events */
            errorInfo_p = solClient_getLastErrorInfo (  );
            /* Error events are output to STDOUT. */
            printf ( "replication_eventCallback() called - %s; subCode %s, responseCode %d, reason %s\n",
                     solClient_session_eventToString ( eventInfo_p->sessionEvent ),
                     solClient_subCodeToString ( errorInfo_p->subCode ), errorInfo_p->responseCode, errorInfo_p->errorStr );
            break;
        
        default:
            /* Unrecognized or deprecated events are output to STDOUT. */
            printf ( "replication_eventCallback() called - %s.  Unrecognized or deprecated event.\n",
                     solClient_session_eventToString ( eventInfo_p->sessionEvent ) );
            break;
    }
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

    int             loop;
    char            binMsg[1024];
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;

    publisherInfoStruct_t pubInfo;


    printf ( "\nreplication.c (Copyright 2010-2020 Solace Corporation. All rights reserved.)\n" );

    memset (&pubInfo, 0, sizeof(pubInfo));

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
     * Initialize the API (and setup logging level)
     *************************************************************************/

    /* solClient needs to be initialized before any other API calls. */
    if ( ( rc = solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_initialize()" );
        goto notInitialized;
    }

    common_printCCSMPversion (  );

    initSigHandler (  );

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
     * Create a Context, and specify that the Context thread should be created 
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

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 replication_eventCallback, &pubInfo, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Publish
     *************************************************************************/
    for ( loop = 0; ( loop < commandOpts.numMsgsToSend ) && !gotCtlC; loop++ ) {

        /*************************************************************************
         * MSG building
         *************************************************************************/

        /* Allocate a message. */
        if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_alloc()" );
            goto sessionConnected;
        }
        /* Set the delivery mode for the message. */
        if ( ( rc = solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDeliveryMode()" );
            goto sessionConnected;
        }
        /* Initialize a binary attachment, and use it as part of the message. */
        memset ( ( void * ) binMsg, 0xab, sizeof ( binMsg ) );
        if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p, binMsg, sizeof ( binMsg ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setBinaryAttachmentPtr()" );
            goto sessionConnected;
        }

        destination.destType = SOLCLIENT_TOPIC_DESTINATION;
        destination.dest = COMMON_MY_SAMPLE_TOPIC;
        if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setDestination()" );
            goto sessionConnected;
        }


        /*************************************************************************
         * MSG sending
         *************************************************************************/

        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_send" );
            break;
        }

        if ( ( rc = solClient_msg_free (&msg_p) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_free" );
            break;
        }

        printf ("Sent %d\n", loop+1);

        sleepInSec ( 1 );

    }

    /*************************************************************************
     * CLEANUP
     *************************************************************************/
    if ( gotCtlC ) {
        printf ( "Got Ctrl-C, cleaning up\n" );
    } 
    /* Sleep to allow last message to be acknowledged. */
    sleepInSec ( 1 );

  sessionConnected:
    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

    if ( pubInfo.rejectedMsgRx_m != 0 ) {
        /*
         * We don't expect rejected messages in this test. Make note.
         */
        printf ( "Test saw '%d' SOLCLIENT_SESSION_EVENT_REJECTED_MSG_ERROR. None expected\n",
             pubInfo.rejectedMsgRx_m );
    }
    if ((  pubInfo.rejectedMsgRx_m +  pubInfo.acknowledgementRx_m ) != loop ) {
        /*
         * We expect to see one response for every message sent.
         */
        printf ( "Test saw '%d' responses (acknowlegement+rejected). Expected '%d'\n",
             pubInfo.rejectedMsgRx_m+pubInfo.acknowledgementRx_m, loop );
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
