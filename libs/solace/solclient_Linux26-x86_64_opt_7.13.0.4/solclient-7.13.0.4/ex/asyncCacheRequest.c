
/** @example ex/asyncCacheRequest.c 
 */

/*
 * This sample demonstrates:
 *  - Creating a cache session.
 *  - Sending an asynchronous cache request.
 *
 * Sample Requirements:
 *  - A Solace appliance running SolOS-TR that has an active cache.
 *  - A cache running and caching on a pattern that matches "my/sample/topic".
 *  - The cache name must be known and passed to this program as a command line
 *    argument.
 *
 * This sample sends an asynchronous cache request. The cache request
 * is not blocking, so the sample application is made to sleep when it reaches 
 * the end of execution. This gives the cache time to respond to the request.
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
 * cache_eventCallback
 *****************************************************************************/
void
cache_eventCallback ( solClient_opaqueSession_pt opaqueSession_p, solCache_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
    printf ( "cache_EventCallback() called: \n"
             "\tevent: %s\n"
             "\ttopic: %s\n"
             "\tresponseCode: (%d) %s\n"
             "\tsubCode: (%d) %s\n"
             "\tcacheRequestId: %llu\n\n",
             solClient_cacheSession_eventToString ( eventInfo_p->cacheEvent ),
             eventInfo_p->topic,
             eventInfo_p->rc, solClient_returnCodeToString ( eventInfo_p->rc ),
             eventInfo_p->subCode, solClient_subCodeToString ( eventInfo_p->subCode ), eventInfo_p->cacheRequestId );

    switch ( eventInfo_p->cacheEvent ) {
        case SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE:
            switch ( eventInfo_p->rc ) {
                case SOLCLIENT_OK:
                    /* Non-error events are logged at the INFO level. */
                    solClient_log ( SOLCLIENT_LOG_INFO, "received event=SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE,rc=SOLCLIENT_OK" )
                            break;
                case SOLCLIENT_INCOMPLETE:
                    switch ( eventInfo_p->subCode ) {
                        case SOLCLIENT_SUBCODE_CACHE_NO_DATA:
                            solClient_log ( SOLCLIENT_LOG_INFO,
                                            "received event=SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE,"
                                            "rc=SOLCLIENT_INCOMPLETE, subCode=%s",
                                            solClient_subCodeToString ( eventInfo_p->subCode ) );
                            break;
                        case SOLCLIENT_SUBCODE_CACHE_SUSPECT_DATA:
                            solClient_log ( SOLCLIENT_LOG_INFO,
                                            "received event=SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE,"
                                            "rc=SOLCLIENT_INCOMPLETE, subCode=%s",
                                            solClient_subCodeToString ( eventInfo_p->subCode ) );
                            break;
                        case SOLCLIENT_SUBCODE_CACHE_TIMEOUT:
                            solClient_log ( SOLCLIENT_LOG_INFO,
                                            "received event=SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE,"
                                            "rc=SOLCLIENT_INCOMPLETE, subCode=%s",
                                            solClient_subCodeToString ( eventInfo_p->subCode ) );
                            break;
                        default:
                            solClient_log ( SOLCLIENT_LOG_NOTICE,
                                            "received event=SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE,"
                                            "rc=SOLCLIENT_INCOMPLETE, with unusual subcode subCode=%d", eventInfo_p->subCode );
                            break;
                    }
                    break;
                case SOLCLIENT_FAIL:
                    solClient_log ( SOLCLIENT_LOG_WARNING,
                                    "received event=SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE,rc=SOLCLIENT_FAIL" )
                            break;
                default:
                    solClient_log ( SOLCLIENT_LOG_NOTICE,
                                    "received event=SOLCACHE_EVENT_REQUEST_COMPLETED_NOTICE, "
                                    "with unusual rc=%d", eventInfo_p->rc );
                    break;
            }
            break;
        default:
            solClient_log ( SOLCLIENT_LOG_WARNING, "received unusual event=%d for cache", eventInfo_p->cacheEvent );
            break;
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

    /* Command Options */
    struct commonOptions commandOpts;

    /* Context */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session */
    solClient_opaqueSession_pt session_p;

    /* Cache Session */
    solClient_opaqueCacheSession_pt cacheSession_p;
    const char     *cacheProps[20];
    int             propIndex = 0;
    solClient_cacheRequestFlags_t cacheFlags;
    solClient_uint64_t cacheRequestId = 1;

    printf ( "\nasyncCacheRequest.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /* Intialize Control-C handling. */
    initSigHandler (  );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    common_initCommandOptions(&commandOpts, 
                               ( USER_PARAM_MASK |
                                CACHE_PARAM_MASK ),    /* required parameters */
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

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient Sessions." );

    if ( ( rc = common_createAndConnectSession ( context_p,
                                                 &session_p,
                                                 common_messageReceivePrintMsgCallback,
                                                 common_eventCallback, NULL, &commandOpts ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_createAndConnectSession()" );
        goto cleanup;
    }

    /*************************************************************************
     * Publish a message (just to make sure there is one cached)
     *************************************************************************/

    if ( ( rc = common_publishMessage ( session_p, commandOpts.destinationName, SOLCLIENT_DELIVERY_MODE_DIRECT ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "common_publishMessage()" );
        goto sessionConnected;
    }

    /*************************************************************************
     * Create a cache session (within the connected Session)
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
     * CACHE REQUEST (asynchronous)
     *************************************************************************/

    printf ( "Sending cache request.\n\n" );

    /* 
     * SOLCLIENT_CACHEREQUEST_FLAGS_LIVEDATA_QUEUE:
     * The cache request completes when the cache response is returned. Live
     * data matching the cache request Topic is queued until the cache request
     * completes. The queued live data is delivered to the application before
     * the cache response message.
     *
     * SOLCLIENT_CACHEREQUEST_FLAGS_NOWAIT_REPLY:
     * By default, solClient_cacheSession_sendCacheRequest blocks until the 
     * cache request completes and then returns the cache request status. If 
     * the cache request flag SOLCLIENT_CACHEREQUEST_FLAGS_NOWAIT_REPLY is 
     * set, solClient_cacheSession_sendCacheRequest returns immediately with 
     * the status SOLCLIENT_IN_PROGRESS, and the cache request status is 
     * returned through a callback when it completes.
     *
     * For more information on other cacheFlags, refer to solCache.h.
     */
    cacheFlags = SOLCLIENT_CACHEREQUEST_FLAGS_LIVEDATA_QUEUE | SOLCLIENT_CACHEREQUEST_FLAGS_NOWAIT_REPLY;

    if ( ( rc = solClient_cacheSession_sendCacheRequest ( cacheSession_p,
                                                          commandOpts.destinationName,
                                                          cacheRequestId,
                                                          cache_eventCallback,
                                                          &session_p, cacheFlags, 0 ) ) != SOLCLIENT_IN_PROGRESS ) {
        common_handleError ( rc, "solClient_cacheSession_sendCacheRequest()" );
        goto cleanupCache;
    }

    printf ( "Cache request sent.\n\n" );

    /*************************************************************************
     * Wait for a response. (The default timeout to wait for a response from the 
     * cache is 10 seconds.)
     *************************************************************************/
    printf ( "Waiting for cache response. Sleeping for 11 seconds.\n\n" );
    sleepInSec ( 11 );
    printf ( "Exiting.\n" );

    /*************************************************************************
     * CLEANUP
     *************************************************************************/
  cleanupCache:
    if ( ( rc = solClient_cacheSession_destroy ( &cacheSession_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cacheSession_destroy()" );
    }

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
