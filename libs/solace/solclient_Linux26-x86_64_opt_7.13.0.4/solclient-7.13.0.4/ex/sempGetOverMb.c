
/** @example ex/sempGetOverMb.c
 */

/*
 * This sample demonstrates:
 *  - Performing a Solace Element Management Protocol (SEMP) request over the
 *    Message Backbone.
 *
 * Sample requirements:
 *  - A Solace appliance running SolOS-TR.
 *  - When running with SolOS-TR 5.3.1 and above. The client's Message VPN must have semp-over-msgbus enabled
 *  - The client's Message VPN must have the SEMP over Message Bus and Show Commands enabled.

 *
 * This sample sends a SEMP request to show all clients currently
 * connected to the appliance. The reply to the request is printed to STDOUT.
 * This is a basic example of SEMP request/reply. Other similar requests 
 * could be used to create a customized appliance monitoring application.
 *
 * Copyright 2009-2020 Solace Corporation. All rights reserved.
 */


/*****************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *****************************************************************************/
#include "os.h"
#include "common.h"



/*****************************************************************************
 * sempRequestRequestAndReply
 *
 * Send a blocking SEMP request to the appliance. The reply is output to STDOUT.
 *****************************************************************************/
static void
sempRequestRequestAndReply ( solClient_opaqueSession_pt session_p, const char *sempVersion )
{
    solClient_returnCode_t rc;
    solClient_opaqueMsg_pt msg_p;
    solClient_opaqueMsg_pt replyMsg_p;
    solClient_destination_t destination;

    solClient_field_t routerName;
    char            sempTopic[SOLCLIENT_BUFINFO_MAX_TOPIC_SIZE + 1];
    char            sempRequest[512];
    void           *sempReplyBuf_p;
    solClient_uint32_t sempReplyBufSize;
    solClient_uint32_t ix;

    /*************************************************************************
     * Create the SEMP request.
     *************************************************************************/
    snprintf(sempRequest, sizeof(sempRequest),
         "<rpc semp-version=\"%s\"><show><client><name>*</name></client></show></rpc>",
         sempVersion);

    /* Allocate a message for requests. */
    if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_alloc()" );
        return;
    }

    /* 
     * Get the hostname of the appliance. The hostname is part of the Topic
     * used to perform SEMP requests.
     */
    if ( ( rc = solClient_session_getCapability ( session_p,
                                                  SOLCLIENT_SESSION_PEER_ROUTER_NAME,
                                                  &routerName, sizeof ( routerName ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_getCapability()" );
        goto freeMsg;
    }

    /* Construct the SEMP topic. */
    snprintf ( sempTopic, sizeof ( sempTopic ), COMMON_SEMP_TOPIC_FORMAT, routerName.value.string );

    /* Set SEMP topic as the destination for the message. */
    destination.destType = SOLCLIENT_TOPIC_DESTINATION;
    destination.dest = sempTopic;

    if ( ( rc = solClient_msg_setDestination ( msg_p, &destination, sizeof ( destination ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setDestination()" );
        goto freeMsg;
    }

    /* Set the binary attachment of the message to the SEMP request. */
    if ( ( rc = solClient_msg_setBinaryAttachmentPtr ( msg_p,
                                                       ( void * ) sempRequest,
                                                       ( solClient_uint32_t ) strlen ( sempRequest ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_setBinaryAttachmentPtr()" );
        goto freeMsg;
    }

    /*************************************************************************
     * Send the SEMP request.
     *************************************************************************/

    /* Send a blocking request. */

    printf ( "REQUEST: %s\n", sempRequest );
    printf ( "REQUEST ADDRESS: %s\n", sempTopic );

    if ( ( rc = solClient_session_sendRequest ( session_p, msg_p, &replyMsg_p, 5000 ) ) == SOLCLIENT_OK ) {

        /*********************************************************************
         * Interpret the SEMP reply.
         *********************************************************************/

        /* The reply is in the binary attachment, so a pointer is returned. */
        if ( ( rc = solClient_msg_getBinaryAttachmentPtr ( replyMsg_p, &sempReplyBuf_p, &sempReplyBufSize ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_getBinaryAttachmentPtr()" );
            goto freeReplyMsg;
        }

        /*
         * The SEMP reply is XML, but not in a string format. It need to be printed 
         * to the screen character by character.
         */
        printf ( "REPLY: " );
        for ( ix = 0; ix < sempReplyBufSize; ix++ ) {
            printf ( "%c", ( ( char * ) sempReplyBuf_p )[ix] );
        }
        printf ( "\n" );

        /*********************************************************************
         * Cleanup.
         *********************************************************************/

      freeReplyMsg:
        /* Done with the reply message, so free it. */
        if ( ( rc = solClient_msg_free ( &replyMsg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_free()" );
            goto freeMsg;
        }
    } else {
        common_handleError ( rc, "solClient_session_sendRequest()" );
        goto freeMsg;
    }
  freeMsg:
    /* Finally, free the request message. */
    if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_free()" );
    }
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
    char                   sempVersion[256];

    /* Command Options */
    struct commonOptions commandOpts;
    char                   positionalParms[] = "\tSempVersion                  Semp version (default 'soltr/5_1').\n";

    /* Context */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session */
    solClient_opaqueSession_pt session_p;


    printf ( "\nsempGetOverMb.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control-C handling. */
    initSigHandler (  );

    /*************************************************************************
     * Parse command options.
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK ),    /* required parameters */
                               ( HOST_PARAM_MASK |
                                PASS_PARAM_MASK |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, positionalParms ) == 0 ) {
        exit(1);
    }
    /*
     * If the user specified the semp version string, override the 
     * default 
     */
    if (optind < argc ) {
        strncpy(sempVersion, argv[optind], sizeof(sempVersion));
    } else {
        strncpy(sempVersion, "soltr/5_1", sizeof(sempVersion));
    }

    /*************************************************************************
     * Initialize the API and setup logging level.
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
    * Create a Context.
    *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient context." );

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
     * Create and connect a Session.
     *************************************************************************/

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient sessions." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceiveCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * SEMP request and reply
     *************************************************************************/

    /* A SEMP request is sent, and the reply is output to the screen. */
    sempRequestRequestAndReply ( session_p, sempVersion );

    /*************************************************************************
     * Cleanup.
     *************************************************************************/

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

    return 0;
}
