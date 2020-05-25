
/** @example ex/adPubAck.c 
 */

/*
 * This sample shows the publishing of Guaranteed messages and how message 
 * acknowledgements are handled.
 *
 * To accomplish this, the publisher makes use of the correlation ptr
 * in each message. The publisher adds a ptr to a correlation structure
 * to the Solace message before sending. Then in the event callback, 
 * the publisher can process SOLCLIENT_SESSION_EVENT_ACKNOWLEDGEMENT and 
 * SOLCLIENT_SESSION_EVENT_REJECTED_MSG_ERROR to determine if the 
 * appliance accepted the Guaranteed message.
 *
 * In this specific sample, the publisher maintains a linked list of
 * outstanding messages not yet acknowledged by the appliance. After sending,
 * the publisher checks to see if any of the messages have been 
 * acknowledged, and, if so, it frees the resources.
 *
 * In the event callback, the original pointer to the correlation structure
 * is passed in as an argument, and the event callback updates the information 
 * to indicate if the message has been acknowledged and whether it was 
 * accepted or rejected.  
 * 
 * For simplicity, this sample treats both message acceptance and 
 * rejection the same way: the message is freed. In real world 
 * applications, the client should decide what to do in the failure
 * scenario.
 *
 * The reason the message is not processed in the event callback 
 * in this sample is because it is not possible to make blocking 
 * calls from within the event callback. In general, it is often 
 * simpler to send messages as blocking, as is done in the publish
 * thread of this sample. So, consequently, if an application 
 * wanted to resend rejected messages, it would have to avoid doing
 * this in the callback or update the code to use non-blocking 
 * sends. This sample chooses to avoid processing the message within
 * the callback.
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
 * @struct messageCorrelationStruct
 *
 * A structure that is used to handle message acknowledgement.
 */
typedef struct messageCorrelationStruct
{
    struct messageCorrelationStruct *next_p;

    int             msgId;                  /**< The message ID. */

    solClient_opaqueMsg_pt msg_p;           /**< The message pointer. */

    BOOL            isAcked;                /**< A flag indicating if the message has been acknowledged by the appliance (either success or rejection). */

    BOOL            isAccepted;             /**< A flag indicating if the message is accepted or rejected by the appliance when acknowledged.*/

} messageCorrelationStruct_t, *messageCorrelationStruct_pt; /**< A pointer to ::messageCorrelationStruct structure of information. */


void
adPubAck_eventCallback ( solClient_opaqueSession_pt opaqueSession_p,
                         solClient_session_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
    solClient_errorInfo_pt errorInfo_p;
    messageCorrelationStruct_pt correlationInfo = ( messageCorrelationStruct_pt ) eventInfo_p->correlation_p;

    switch ( eventInfo_p->sessionEvent ) {
        case SOLCLIENT_SESSION_EVENT_ACKNOWLEDGEMENT:
            /* Non-error events are logged at the INFO level. */
            solClient_log ( SOLCLIENT_LOG_INFO,
                            "adPubAck_eventCallback() called - %s\n",
                            solClient_session_eventToString ( eventInfo_p->sessionEvent ));


            printf ( "adPubAck_eventCallback() correlation info - ID: %i\n", correlationInfo->msgId );

            correlationInfo->isAcked = TRUE;
            correlationInfo->isAccepted = TRUE;
            break;

        case SOLCLIENT_SESSION_EVENT_REJECTED_MSG_ERROR:
            /* Extra error information is available on error events */
            errorInfo_p = solClient_getLastErrorInfo (  );
            /* Error events are output to STDOUT. */
            printf ( "adPubAck_eventCallback() called - %s; subCode %s, responseCode %d, reason %s\n",
                     solClient_session_eventToString ( eventInfo_p->sessionEvent ),
                     solClient_subCodeToString ( errorInfo_p->subCode ), errorInfo_p->responseCode, errorInfo_p->errorStr );


            printf ( "adPubAck_eventCallback() correlation info - ID: %i\n", correlationInfo->msgId );

            correlationInfo->isAcked = TRUE;
            correlationInfo->isAccepted = FALSE;
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
                            "adPubAck_eventCallback() called - %s\n",
                            solClient_session_eventToString ( eventInfo_p->sessionEvent ));
            break;

        case SOLCLIENT_SESSION_EVENT_DOWN_ERROR:
        case SOLCLIENT_SESSION_EVENT_CONNECT_FAILED_ERROR:
        case SOLCLIENT_SESSION_EVENT_SUBSCRIPTION_ERROR:
        case SOLCLIENT_SESSION_EVENT_TE_UNSUBSCRIBE_ERROR:
        case SOLCLIENT_SESSION_EVENT_PROVISION_ERROR:

            /* Extra error information is available on error events */
            errorInfo_p = solClient_getLastErrorInfo (  );
            /* Error events are output to STDOUT. */
            printf ( "adPubAck_eventCallback() called - %s; subCode %s, responseCode %d, reason %s\n",
                     solClient_session_eventToString ( eventInfo_p->sessionEvent ),
                     solClient_subCodeToString ( errorInfo_p->subCode ), errorInfo_p->responseCode, errorInfo_p->errorStr );
            break;
        default:
            /* Unrecognized or deprecated events are output to STDOUT. */
            printf ( "adPubAck_eventCallback() called - %s.  Unrecognized or deprecated event.\n",
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

    messageCorrelationStruct_pt msgMemoryItem_p = NULL;
    messageCorrelationStruct_pt msgMemoryListHead_p = NULL;
    messageCorrelationStruct_pt msgMemoryListTail_p = NULL;


    printf ( "\nadPubAck.c (Copyright 2010-2020 Solace Corporation. All rights reserved.)\n" );

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
                                                 adPubAck_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
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
         * MSG ACK correlation
         *************************************************************************/

        msgMemoryItem_p = ( messageCorrelationStruct_pt ) malloc ( sizeof ( messageCorrelationStruct_t ) );

        /* Store the message information in message memory array. */
        msgMemoryItem_p->next_p = NULL;
        msgMemoryItem_p->msgId = loop;
        msgMemoryItem_p->msg_p = msg_p;
        msgMemoryItem_p->isAcked = FALSE;
        msgMemoryItem_p->isAccepted = FALSE;

        if ( msgMemoryListTail_p != NULL ) {
            msgMemoryListTail_p->next_p = msgMemoryItem_p;
        }

        if ( msgMemoryListHead_p == NULL ) {
            msgMemoryListHead_p = msgMemoryItem_p;
        }

        msgMemoryListTail_p = msgMemoryItem_p;

        /*
         * For correlation to take effect, it must be set on the message prior to 
         * calling send. Note: the size parameter is ignored in the API.
         */
        if ( ( rc = solClient_msg_setCorrelationTagPtr ( msg_p, msgMemoryItem_p, sizeof ( *msgMemoryItem_p ) ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setCorrelationTag()" );
            goto sessionConnected;
        }


        /*************************************************************************
         * MSG sending
         *************************************************************************/

        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_send" );
            break;
        }

        sleepInSec ( 1 );

        /*************************************************************************
         * MSG cleanup by processing ACKs
         *************************************************************************/
        while ( ( msgMemoryListHead_p != NULL ) && msgMemoryListHead_p->isAcked ) {
            printf ( "Freeing memory for message %i, Result: Acked (%i), Accepted (%i)\n",
                     msgMemoryListHead_p->msgId, msgMemoryListHead_p->isAcked, msgMemoryListHead_p->isAccepted );
            msgMemoryItem_p = msgMemoryListHead_p;

            if ( ( msgMemoryListHead_p = msgMemoryListHead_p->next_p ) == NULL ) {
                /* list is now empty */
                msgMemoryListTail_p = NULL;
            }

            solClient_msg_free ( &( msgMemoryItem_p->msg_p ) );
            free ( msgMemoryItem_p );
        }
    }

    /*************************************************************************
     * CLEANUP
     *************************************************************************/
    if ( gotCtlC ) {
        printf ( "Got Ctrl-C, cleaning up\n" );
    } else {
        /* Sleep to allow last message to be acknowledged. */
        sleepInSec ( 1 );
    }

  sessionConnected:
    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

  cleanup:
    /* Cleanup any messages that are still left. */
    while ( msgMemoryListHead_p != NULL ) {
        printf ( "Freeing memory for message %i, Result: Acked (%i), Accepted (%i)\n",
                 msgMemoryListHead_p->msgId, msgMemoryListHead_p->isAcked, msgMemoryListHead_p->isAccepted );
        msgMemoryItem_p = msgMemoryListHead_p;

        if ( ( msgMemoryListHead_p = msgMemoryListHead_p->next_p ) == NULL ) {
            /* list is now empty */
            msgMemoryListTail_p = NULL;
        }

        solClient_msg_free ( &( msgMemoryItem_p->msg_p ) );
        free ( msgMemoryItem_p );
    }

    /* Cleanup solClient. */
    if ( ( rc = solClient_cleanup (  ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cleanup()" );
    }
    goto notInitialized;

  notInitialized:
    /* Nothing to do - just exit. */

    return 0;
}                               //End main()
