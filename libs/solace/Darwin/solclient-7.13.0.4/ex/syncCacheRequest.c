
/** @example ex/syncCacheRequest.c
 */

/*
 * This sample demonstrates:
 *  - Creating a cache session.
 *  - Sending an synchronous cache request.
 *
 * Sample Requirements:
 *  - A appliance running SolOS-TR with an active cache.
 *  - A cache running and caching on a pattern that matches "my/sample/topic".
 *  - The cache name must be known and passed to this program as a command line
 *    argument.
 *
 * In this sample, a synchronous cache request is sent. The cache request 
 * is blocking, so it is possible to see that the cache response is processed 
 * before the application continues with its execution.
 *
 * Cached messages returned as a result of the cache request are handled by
 * the Session's message receive callback in the normal manner. This sample
 * uses a callback that simply dumps the message to STDOUT.
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
 * main
 * 
 * The entry point to the application.
 *****************************************************************************/
int
main ( int argc, char *argv[] )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;

    /* Command Options. */
    struct commonOptions commandOpts;

    /* Context. */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session. */
    solClient_opaqueSession_pt session_p;

    /* Cache Session. */
    solClient_opaqueCacheSession_pt cacheSession_p;
    const char     *cacheProps[20];
    int             propIndex = 0;
    solClient_cacheRequestFlags_t cacheFlags;
    solClient_uint64_t cacheRequestId = 1;

    printf ( "\nsyncCacheRequest.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control-C handling. */
    initSigHandler (  );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK |
                                CACHE_PARAM_MASK),    /* required parameters */
                               ( HOST_PARAM_MASK |
                                DEST_PARAM_MASK | 
                                PASS_PARAM_MASK |
                                LOG_LEVEL_MASK |
                                USE_GSS_MASK |
                                ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, NULL ) == 0 ) {
        exit(1);
    }

    if ( commandOpts.destinationName[0] == ( char ) 0 ) {
        strncpy (commandOpts.destinationName, COMMON_MY_SAMPLE_TOPIC, sizeof(commandOpts.destinationName));
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

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient sessions." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Publish a message (just to make sure there is one there cached)
     *************************************************************************/

    if ( ( rc = common_publishMessage ( session_p, commandOpts.destinationName, SOLCLIENT_DELIVERY_MODE_DIRECT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_publishMessage()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * CREATE A CACHE SESSION (within the connected Session)
     *************************************************************************/

    propIndex = 0;

    cacheProps[propIndex++] = SOLCLIENT_CACHESESSION_PROP_CACHE_NAME;
    cacheProps[propIndex++] = commandOpts.cacheName;

    cacheProps[propIndex] = NULL;

    if ( ( rc = solClient_session_createCacheSession ( ( const char *const * ) cacheProps,
                                                       session_p, &cacheSession_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_createCacheSession" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Cache the request (synchronous)
     *************************************************************************/

    printf ( "Sending cache request.\n\n" );

    /* 
     * SOLCLIENT_CACHEREQUEST_FLAGS_LIVEDATA_QUEUE:
     * The cache request completes when the cache response is returned. Live
     * data matching the cache request topic is queued until the cache request
     * completes. The queued live data is delivered to the application before
     * the cache response message.
     *
     * For more information on other cacheFlags, refer to solCache.h.
     */
    cacheFlags = SOLCLIENT_CACHEREQUEST_FLAGS_LIVEDATA_QUEUE;

    if ( ( rc = solClient_cacheSession_sendCacheRequest ( cacheSession_p,
                                                          commandOpts.destinationName,
                                                          cacheRequestId, NULL, &session_p, cacheFlags, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cacheSession_sendCacheRequest()" );
        goto cleanupCache;
    }

    printf ( "Cache request sent.\n\n" );

    /*************************************************************************
     * Cleanup
     *************************************************************************/

  cleanupCache:
    if ( ( rc = solClient_cacheSession_destroy ( &cacheSession_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cacheSession_destroy()" );
    }

    printf ( "Exiting.\n" );

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
