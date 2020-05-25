
/** @example ex/perfTest.c
 *  This sample demonstrates a performance-oriented test client.
 */

/*
 * This example is capable of performing the following performance tests:
 *    1. Publisher throughput - will send a number of messages at a given rate to a Topic.
 *       Program arguments of interest:
 *           NUM_MSGS - specify the number of messages to publish
 *           MSG_RATE - publish message rate (per second)
 *           MSG_SIZE - size of binary payload portion of message
 *           PUB_SUB_MODE - must be 'p'
 *           MULTI_SEND_MODE - "true" to send multiple messages per send call.
 *       (see pubThread() and pubThreadSendMultiple()).
 *    2. Subscriber throughput - will subscribe to messages on a Topic
 *       Program arguments of interest:
 *           NUM_MSGS - specify the number of messages expected to receive. The program
 *                      will exit after receiving this number of messages or more, or
 *                      if the user hits Ctrl+C, whichever comes first.
 *           PUB_SUB_MODE - must be 's'
 *       (see msgRxCallbackFunc()).
 *    3. Publisher/subscriber throughput - will set up a publisher and subscriber to a
 *       Topic.
 *       Program arguments of interest:
 *           NUM_MSGS - specify the number of messages to publish and expected to receive.
 *           MSG_RATE - publish message rate (per second)
 *           MSG_SIZE - size of binary payload portion of message
 *           PUB_SUB_MODE - must be 'b'
 *           MULTI_SEND_MODE - "true" to send multiple messages per send call.
 *       (see pubThread(), pubThreadSendMultiple(), and msgRxCallbackFunc()).
 *
 * At the end of each test, performance statistics are printed (see printStats()).
 *
 * Copyright 2007-2020 Solace Corporation. All rights reserved.
 */

#include <signal.h>

/**************************************************************************
 *  For Windows builds, os.h should always be included first to ensure that
 *  _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 **************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "common.h"

#if defined(__cplusplus)
extern          "C"
{
#endif                          /* __cplusplus */

static int      exitEarly_s = 0;
static int      rxDone_s = 0;
static int      rxTimeout_s = 0;
static int      usePub_s = 1;
static int      useSub_s = 1;
static int      multiSend_s = 0;
static int      binaryPayloadSize_s = 100;  /* default binary payload size of 100 bytes if not specified */
static int      sendPersistent_s = 0;
static unsigned int numRx_s = 0;
static unsigned int msgRate_s;
static unsigned int msgNum_s;
static char    *publishTopic_ps;

/*
* fn printStats()
* param session_p Pointer to the Session to print stats for.
*
* This function prints rx and tx stats for the given Session to stdout.
* The Session statistics are cleared before returning.
*/
static void     printStats ( solClient_opaqueSession_pt session_p )
{
    solClient_stats_t rxStats[SOLCLIENT_STATS_RX_NUM_STATS];
    solClient_stats_t txStats[SOLCLIENT_STATS_TX_NUM_STATS];
    solClient_returnCode_t rc;

    if ( ( rc = solClient_session_getRxStats ( session_p, rxStats, SOLCLIENT_STATS_RX_NUM_STATS ) )
                      != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_getRxStats()" );
        return;
    }

    if ( ( rc = solClient_session_getTxStats ( session_p, txStats, SOLCLIENT_STATS_TX_NUM_STATS ) )
                      != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_getTxStats()" );
        return;
    }

    printf ( "\n" "Tx msgs = %lld; Tx bytes = %lld\n"
             "Rx msgs = %lld, Rx bytes = %lld\n"
             "Avg bytes per read = %lld\n"
             "Rx discard indications = %lld\n"
             "Rx discards due to unrecognized parameter in header = %lld\n"
             "Rx discards due to message too big = %lld\n"
             "Tx would block = %lld\n" "Tx socket full = %lld\n" "\n",
             txStats[SOLCLIENT_STATS_TX_TOTAL_DATA_MSGS],
             txStats[SOLCLIENT_STATS_TX_TOTAL_DATA_BYTES],
             rxStats[SOLCLIENT_STATS_RX_DIRECT_MSGS],
             rxStats[SOLCLIENT_STATS_RX_DIRECT_BYTES],
             rxStats[SOLCLIENT_STATS_RX_READS] ?
             rxStats[SOLCLIENT_STATS_RX_DIRECT_BYTES] / rxStats[SOLCLIENT_STATS_RX_READS] : 0LL,
             rxStats[SOLCLIENT_STATS_RX_DISCARD_IND],
             rxStats[SOLCLIENT_STATS_RX_DISCARD_SMF_UNKNOWN_ELEMENT],
             rxStats[SOLCLIENT_STATS_RX_DISCARD_MSG_TOO_BIG],
             txStats[SOLCLIENT_STATS_TX_WOULD_BLOCK], txStats[SOLCLIENT_STATS_TX_SOCKET_FULL] );
    // solClient_session_logStats (session_p, SOLCLIENT_LOG_NOTICE);

    /* Clear statistics */
    if ( ( rc = solClient_session_clearStats ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_clearStats()" );
    }
}

/*
* fn waitRxDoneCallbackFunc()
* param opaqueContext_p Pointer to Context under which the timer was started previously.
* param user_p Pointer to opaque user data provided when timer started (null in this case).
*
* This function is used to handle the case where a timeout occurs
* waiting for all messages to be received. The number of messages 
* expected to be received is specified in the program argument NUM_MSGS. 
*/
static void waitRxDoneCallbackFunc ( solClient_opaqueContext_pt opaqueContext_p, void *user_p )
{
    solClient_log ( SOLCLIENT_LOG_ERROR, "Timed out waiting for message receive to finish" );
    /* Flag to main loop to stop waiting */
    rxTimeout_s = 1;
}

/*****************************************************************************
 * messageReceiveCallback
 *
 * This function is called when a message is received. It increments the received
 * message counter and sets a flag if there are no more messages expected to
 * be received. The number of messages expected to receive is the NUM_MSGS program
 * parameter. 
 */
solClient_rxMsgCallback_returnCode_t
messageReceiveCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    numRx_s++;
    if ( numRx_s >= msgNum_s ) {
        /* Flag to main loop to stop waiting */
        rxDone_s = 1;
    }
    return SOLCLIENT_CALLBACK_OK;
}

/*
 * fn pubThread()
 * param session_p session to use for publishing.
 *
 * This function does the publishing (in its own thread) of the requested number of 
 * messages at the requested rate and then computes the elapsed time.
 */
threadRetType   pubThread ( void *session_p )
{
    const unsigned int groupSize = 10;
    unsigned int    txCount = 0;
    unsigned int    groupCount = 0;
    char           *binary_p;
    long long       startTime;
    long long       targetTime;
    long long       currentTime;
    long long       timeDiff;
    long long       elapsedTime;
    long long       usPerGroup;
    long double     usPerMsg;
    solClient_returnCode_t sendRc;
    solClient_opaqueMsg_pt msg_p;
    solClient_errorInfo_pt errorInfo_p;

    binary_p = ( char * ) malloc ( binaryPayloadSize_s );
    if ( binary_p == NULL ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Could not malloc %d bytes", binaryPayloadSize_s );
        return DEFAULT_THREAD_RETURN_ARG;
    }
    memset ( binary_p, 0, binaryPayloadSize_s );

    if ( solClient_msg_alloc ( &msg_p ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Could not allocate msg" );
        free (binary_p);
        return DEFAULT_THREAD_RETURN_ARG;
    }

    usPerMsg = ( long double ) 1000000.0 / ( long double ) msgRate_s;
    usPerGroup = ( long long ) ( usPerMsg * ( long double ) groupSize );

    if ( solClient_msg_setBinaryAttachmentPtr ( msg_p, binary_p, binaryPayloadSize_s ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Could not set binary attachment in msg" );
        solClient_msg_free ( &msg_p );
        free (binary_p);
        return DEFAULT_THREAD_RETURN_ARG;
    }
    if ( solClient_msg_setTopicPtr ( msg_p, publishTopic_ps ) != SOLCLIENT_OK ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Could not set Topic in msg" );
        solClient_msg_free ( &msg_p );
        free (binary_p);
        return DEFAULT_THREAD_RETURN_ARG;
    }
    if ( sendPersistent_s ) {
        if ( solClient_msg_setDeliveryMode ( msg_p, SOLCLIENT_DELIVERY_MODE_PERSISTENT ) != SOLCLIENT_OK ) {
            solClient_log ( SOLCLIENT_LOG_ERROR, "Could not set delivery mode in msg" );
            solClient_msg_free ( &msg_p );
            free (binary_p);
            return DEFAULT_THREAD_RETURN_ARG;
        }
    }

    startTime = getTimeInUs (  );
    targetTime = startTime + usPerGroup;
    while ( ( txCount < msgNum_s ) && ( !exitEarly_s ) ) {
        sendRc = solClient_session_sendMsg ( session_p, msg_p );
        if ( sendRc != SOLCLIENT_OK ) {
            errorInfo_p = solClient_getLastErrorInfo (  );
            if ( errorInfo_p != NULL ) {
                solClient_log ( SOLCLIENT_LOG_WARNING,
                                "solClient_session_sendMsg() failed (%s) subCode (%d:'%s'), error %s",
                                solClient_returnCodeToString ( sendRc ),
                                errorInfo_p->subCode, solClient_subCodeToString ( errorInfo_p->subCode ),
                                errorInfo_p->errorStr );
            }
        } else {
            txCount++;
            groupCount++;
            if ( groupCount >= groupSize ) {
                groupCount = 0;
                currentTime = getTimeInUs (  );
                timeDiff = targetTime - currentTime;
                if ( timeDiff > 1000 ) {
                    sleepInUs ( ( int ) ( timeDiff + 500 ) );
                } else if ( timeDiff < ( long long ) ( -10000 ) ) {
                    /* Fell too far behind; reset time base so we do not burst for too
                     * long */
                    targetTime = currentTime;
                }
                targetTime += usPerGroup;
            }

        }
    }

    solClient_msg_free ( &msg_p );
    free (binary_p);

    elapsedTime = getTimeInUs (  ) - startTime;
    printf ( "\nSent %d msgs in %lld usec; rate of %lu messages/sec\n\n",
             txCount, elapsedTime, ( long unsigned ) ( ( long double ) txCount /
                                                       ( ( long double ) elapsedTime / ( long double ) 1000000.0 ) ) );

    return DEFAULT_THREAD_RETURN_ARG;
}


/*
 * fn pubThreadSendMultiple() 
 * param session_p session to use for publishing.
 *
 * Sends multiple messages on the specified Session, which is more efficient than multiple
 * calls to solClient_session_sendMsg().
 * The purpose of this example is to demonstrate an efficient way for applications that
 * have several messages to send in one shot. In that case, bundling N (defined as
 * GROUP_SIZE in in this example) messages into one send through the
 * solClient_session_sendMultipleMsg(..) will result in one vectored socket write.
 * In this case, it is recommened that the Session property SOLCLIENT_SESSION_PROP_TCP_NODELAY
 * be enabled, since multiple messages are sent at once onto the underlying TCP connection, and
 * so there is no need to have the operating system carry out the TCP delay algorithm to cause
 * fuller packets.
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define GROUP_SIZE 10
#endif

threadRetType   pubThreadSendMultiple ( void *session_p )
{
    unsigned int    txCount = 0;
    char           *binary_p;
    unsigned int    numToSend;
    unsigned int    numWritten;
    long long       startTime;
    long long       targetTime;
    long long       currentTime;
    long long       timeDiff;
    long long       elapsedTime;
    long long       usPerGroup;
    long double     usPerMsg;
    solClient_returnCode_t sendRc;
    int             loop;
    solClient_opaqueMsg_pt msgArray[GROUP_SIZE];

    binary_p = ( char * ) malloc ( binaryPayloadSize_s );
    if ( binary_p == NULL ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "Could not malloc %d bytes", binaryPayloadSize_s );
        return DEFAULT_THREAD_RETURN_ARG;
    }
    memset ( binary_p, 0, binaryPayloadSize_s );
    memset ( msgArray, 0, sizeof ( msgArray ) );

    usPerMsg = ( long double ) 1000000.0 / ( long double ) msgRate_s;
    usPerGroup = ( long long ) ( usPerMsg * ( long double ) GROUP_SIZE );

    for ( loop = 0; loop < GROUP_SIZE; loop++ ) {
        if ( solClient_msg_alloc ( &msgArray[loop] ) != SOLCLIENT_OK ) {
            solClient_log ( SOLCLIENT_LOG_ERROR, "Could not allocate msg # %d", loop );
            goto releaseMsg;
        }
        if ( solClient_msg_setBinaryAttachmentPtr ( msgArray[loop], binary_p, binaryPayloadSize_s ) != SOLCLIENT_OK ) {
            solClient_log ( SOLCLIENT_LOG_ERROR, "Could not set binary attachment in msg" );
            goto releaseMsg;
        }
        if ( solClient_msg_setTopicPtr ( msgArray[loop], publishTopic_ps ) != SOLCLIENT_OK ) {
            solClient_log ( SOLCLIENT_LOG_ERROR, "Could not set topic in msg" );
            goto releaseMsg;
        }
    }

    startTime = getTimeInUs (  );
    targetTime = startTime + usPerGroup;
    while ( ( txCount < msgNum_s ) && ( !exitEarly_s ) ) {
        numToSend = msgNum_s - txCount;
        if ( numToSend > GROUP_SIZE ) {
            numToSend = GROUP_SIZE;
        }
        sendRc = solClient_session_sendMultipleMsg ( session_p, msgArray, numToSend, &numWritten );
        if ( sendRc != SOLCLIENT_OK ) {
            printf ( "Could not send multiple\n" );
            break;
        } else {
            txCount += numToSend;
            currentTime = getTimeInUs (  );
            timeDiff = targetTime - currentTime;
            if ( timeDiff > 1000 ) {
                sleepInUs ( ( int ) ( timeDiff + 500 ) );
            } else if ( timeDiff < ( long long ) ( -10000 ) ) {
                /* Fell too far behind; reset time base so we do not burst for too
                 * long */
                targetTime = currentTime;
            }
            targetTime += usPerGroup;
        }
    }

    elapsedTime = getTimeInUs (  ) - startTime;
    printf ( "\nSent %d msgs in batches of %d in %lld usec; rate of %lu messages/sec\n\n",
             txCount, GROUP_SIZE, elapsedTime, ( long unsigned ) ( ( long double ) txCount /
                                                       ( ( long double ) elapsedTime / ( long double ) 1000000.0 ) ) );

  releaseMsg:
    for ( loop = 0; loop < GROUP_SIZE; loop++ ) {
        if ( msgArray[loop] != NULL ) {
            if ( solClient_msg_free ( &msgArray[loop] ) != SOLCLIENT_OK ) {
                solClient_log ( SOLCLIENT_LOG_ERROR, "Could not release msg # %d", loop );
                break;
            }
        }
    }
    return DEFAULT_THREAD_RETURN_ARG;
}

static void     sigHandler ( int sigNum )
{
    if ( exitEarly_s == 0 ) {
        exitEarly_s = 1;
    } else {
        exit ( 0 );
    }
}

/*
 * fn main() 
 *
 * The entry point to the application.
 */
int main ( int argc, char *argv[] )
{
    char            positionalParms[] =
            "\tMSG_SIZE        the size of the binary payload for published messages; default is 100 bytes\n"
            "\tPUB_SUB_MODE    (default 'b') is one of \n"
            "\t\ts: for subscribers only\n"
            "\t\tp[n]: for 'n' publishers only (default 1)\n"
            "\t\tP[n]: for 'n' persistent publishers (default 1)\n"
            "\t\tb[n]: for 'n' publishers (default 1) and 1 subscribers\n"
            "\t\tB[n]: for 'n' persistent publishers (default 1) and 1 subscribers\n"
            "\tTCP_NO_DELAY is one of\n"
            "\t\ttrue\n"
            "\t\tfalse (default)\n"
            "\tMULTI_SEND_MODE is whether to use the solClient_session_sendMultipleMsg() function. \n"
            "\t\tNOTE: messages sent in MULTI_SEND_MODE are always sent direct.\n"
            "\t\ttrue\n" "\t\tfalse (default)\n";
    const char     *sessionProps[40];
    char           *noDelayVal_p = "0";
    char           *noDelay_p = "false";
    char           *multiSend_p = "false";
    char            subTopic[] = "level1/level2/level3/level4/>";
    char           *subTopic_p;
    char            pubTopic[] = "level1/level2/level3/level4/level5";
    const char     *pubSub_p;
    solClient_opaqueSession_pt session_p;
    long long       startTime;
    long long       endTime;
    int             propIndex;
    int             numThread = 1;
    long long       userTime;
    long long       sysTime;
    long long       elapsedTime;
    solClient_version_info_pt versionInfo_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    solClient_context_timerId_t timerId;
    solClient_returnCode_t rc = SOLCLIENT_OK;
    contextThreadInfo_t contextThreadInfo;
    THREAD_HANDLE_T pubThreadHandle[100];
    struct commonOptions commandOpts;
    int             loop;

    signal ( SIGINT, sigHandler );

    printf ( "\nperfTest.c (Copyright 2007-2020 Solace Corporation. All rights reserved.)\n" );

    /* Initialize solClient */
    if ( ( rc = solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_initialize()" );
        goto notInitialized;
    }

    common_printCCSMPversion (  );

    /* Process command line arguments. */
    /* Check arguments. */
    common_initCommandOptions(&commandOpts, 
                           ( USER_PARAM_MASK ),    /* required parameters */
                           ( HOST_PARAM_MASK | 
                            DEST_PARAM_MASK | 
                            PASS_PARAM_MASK |
                            NUM_MSGS_MASK  |
                            MSG_RATE_MASK  |
                            LOG_LEVEL_MASK |
                            USE_GSS_MASK |
                            ZIP_LEVEL_MASK));                       /* optional parameters */
    if ( common_parseCommandOptions ( argc, argv, &commandOpts, positionalParms ) == 0 ) {
        exit(1);
    }

    msgNum_s = commandOpts.numMsgsToSend;
    msgRate_s = commandOpts.msgRate;

    if ( commandOpts.destinationName[0] == ( char ) 0 ) {
        publishTopic_ps = pubTopic;
        subTopic_p = subTopic;
    } else {
        publishTopic_ps = commandOpts.destinationName;
        subTopic_p = commandOpts.destinationName;
    }

    /* Optional parameters. */

    /* Binary payload size option.  */
    if ( optind < argc ) {
        binaryPayloadSize_s = atoi ( argv[optind] );
    }
    /* Publisher only, subscriber only, or both publisher and subscriber option. */
    pubSub_p = "Pub and Sub";
    if ( ( optind + 1 ) < argc ) {
        if ( *( argv[optind + 1] ) == 'p' ) {
            useSub_s = 0;   /* pub only */
            pubSub_p = "Pub only";
        } else if ( *( argv[optind + 1] ) == 'P' ) {
            useSub_s = 0;   /* pub only */
            pubSub_p = "Pub only";
            sendPersistent_s = 1;
        } else if ( *( argv[optind + 1] ) == 's' ) {
            usePub_s = 0;   /* sub only */
            pubSub_p = "Sub only";
        } else if ( *( argv[optind + 1] ) == 'b' ) {
            pubSub_p = "Pub and Sub";
        } else if ( *( argv[optind + 1] ) == 'B' ) {
            pubSub_p = "Pub and Sub";
            sendPersistent_s = 1;
        } else {
            printf ( "Error: Unknown PUB_SUB_MODE value \"%s\"\n", argv[optind + 1] );
            goto notInitialized;
        }
        if ( *( ( argv[optind + 1] ) + 1 ) != ( char ) 0 ) {
            numThread = atoi ( ( argv[optind + 1] ) + 1 );
            if ( numThread <= 0 ) {
                printf ( "Error: PUB_SUB_MODE (%s) is not 'p', 's', or 'b' optionally followed by an integer\n",
                         argv[optind + 1] );
                goto notInitialized;
            }
            if ( numThread > 100 ) {
                printf ( "Warning: maximum 100 publisher threads supported, reducing %d to 100\n", numThread );
                numThread = 100;
            }
            if ( usePub_s == 0 ) {
                printf ( "Warning: %d publishers ignored in subscriber only mode\n", numThread );
            }
        }
    }
    /* TCP no delay option. */
    if ( ( optind + 2 ) < argc ) {
        if ( strcasecmp ( argv[optind + 2], "false" ) == 0 ) {
            noDelayVal_p = "0";
            noDelay_p = argv[optind + 2];
        } else if ( strcasecmp ( argv[optind + 2], "true" ) == 0 ) {
            noDelayVal_p = "1";
            noDelay_p = argv[optind + 2];
        } else {
            printf ( "Error: Unknown TCP_NO_DELAY value \"%s\"\n", argv[optind + 2] );
            goto notInitialized;
        }
    }
    /* Use multi-message send option. */
    if ( ( optind + 3 ) < argc ) {
        if ( strcasecmp ( argv[optind + 3], "false" ) == 0 ) {
            multiSend_s = 0;
            multiSend_p = argv[optind + 3];
        } else if ( strcasecmp ( argv[optind + 3], "true" ) == 0 ) {
            multiSend_s = 1;
            multiSend_p = argv[optind + 3];
        } else {
            printf ( "Error: Unknown MULTI_SEND_MODE value \"%s\"\n", argv[optind + 3] );
            goto notInitialized;
        }
    }

    printf ( "APPLIANCE_IP: %s,  APPLIANCE_USERNAME: %s, NUM_MSGS: %d,  MSG_RATE: %d, MSG_SIZE: %d, PUB_SUB_MODE %d %s threads, TCP_NO_DELAY: %s, MULTI_SEND_MODE: %s\n",
             commandOpts.targetHost, commandOpts.username, msgNum_s, msgRate_s, binaryPayloadSize_s, numThread, pubSub_p,
             noDelay_p, multiSend_p );

    /* Print version information. */
    if ( ( rc = solClient_version_get ( &versionInfo_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_version_get()" );
        goto notInitialized;
    }

    /* Set the log level for application-generated logs. */
    solClient_log_setFilterLevel ( SOLCLIENT_LOG_CATEGORY_ALL, commandOpts.logLevel );


    startTime = getTimeInUs (  );

    /* Create a Context to use for the Session. */
    solClient_log ( SOLCLIENT_LOG_DEBUG, "creating solClient context" );
    if ( ( rc = solClient_context_create ( NULL, &contextThreadInfo.context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) ) )
         != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_create()" );
        goto initialized;
    }

    /* Start the Context thread. */
    solClient_log ( SOLCLIENT_LOG_DEBUG, "starting solClient context thread" );
    if ( !common_startContextThread ( &contextThreadInfo ) ) {
        solClient_log ( SOLCLIENT_LOG_ERROR, "common_startContextThread() failed" );
        goto contextCreated;
    }

    /* Create Session for sending/receiving messages. */
    propIndex = 0;
    if ( commandOpts.targetHost[0] != (char) 0 ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = commandOpts.targetHost;
    }

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = commandOpts.username;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = commandOpts.password;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_TCP_NODELAY;
    sessionProps[propIndex++] = noDelayVal_p;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SUBSCRIBE_BLOCKING;
    sessionProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_CONNECT_BLOCKING;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_CONNECT_RETRIES;
    sessionProps[propIndex++] = "3";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES;
    sessionProps[propIndex++] = "3";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
    /*
     * If publishing to a appliance running SolOS-Topic Routing (TR), a Message VPN
     * must be specified.
     */
    if ( commandOpts.vpn[0] ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
        sessionProps[propIndex++] = commandOpts.vpn;
    }
    /*
     * Set the compression level 
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_COMPRESSION_LEVEL;
    sessionProps[propIndex++] = ( commandOpts.enableCompression ) ? "9" : "0";


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

    sessionProps[propIndex] = NULL;

    sessionFuncInfo.rxMsgInfo.callback_p = messageReceiveCallback;
    sessionFuncInfo.rxMsgInfo.user_p = ( void * ) NULL;
    sessionFuncInfo.eventInfo.callback_p = common_eventCallback;
    sessionFuncInfo.eventInfo.user_p = ( void * ) NULL;
    solClient_log ( SOLCLIENT_LOG_DEBUG, "creating solClient session" );
    if ( ( rc = solClient_session_create ( sessionProps,
                                           contextThreadInfo.context_p,
                                           &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) )
         != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto contextThreadCreated;
    }

    /*
     *  We have topic subscription reapply enabled so we can add our subscriptions
     *  before connecting.  Then we know all will have been re-applied when the 
     *  blocking connect returns.
     *
     *  This is important for peer-to-peer (IPC) connections as we want to
     *  be sure the subscription is sent to our peer before the peer begins
     *  publishing.
     */
    if ( useSub_s ) {
        /* Do not Wait for confirmation that the subscription has been applied.
         * Setting SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM will cause 
         * solClient_session_topicSubscribeExt() to fail when the session is
         * not yet established. */
        if ( (  rc = solClient_session_topicSubscribeExt ( session_p,
                                                          0,
                                                          subTopic_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_topicSubscribeExt()" );
            goto sessionConnected;
        }
    }
    /* Connect the Session. */
    solClient_log ( SOLCLIENT_LOG_DEBUG, "connecting solClient session" );
    if ( ( rc = solClient_session_connect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
        goto sessionCreated;
    }


    if ( usePub_s ) {
        for ( loop = 0; loop < numThread; loop++ ) {
            if ( multiSend_s ) {
                if ( ( pubThreadHandle[loop] = startThread ( pubThreadSendMultiple,
                                                             ( void * ) session_p ) ) == _NULL_THREAD_ID ) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "could not create publisher thread" );
                    goto sessionConnected;
                }
            } else {
                if ( ( pubThreadHandle[loop] = startThread ( pubThread, ( void * ) session_p ) ) == _NULL_THREAD_ID ) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "could not create publisher thread" );
                    goto sessionConnected;
                }
            }
        }
        for ( loop = 0; loop < numThread; loop++ ) {
            waitOnThread ( pubThreadHandle[loop] );
        }
    }

    if ( exitEarly_s ) {
        goto sessionConnected;
    }

    if ( useSub_s ) {
        if ( usePub_s ) {
            /* In pubsub mode */
            printf ( "Waiting up to 1 second for subscriber to receive all messages...\n" );
            if ( ( rc = solClient_context_startTimer ( contextThreadInfo.context_p,
                                                       SOLCLIENT_CONTEXT_TIMER_ONE_SHOT,
                                                       1000, waitRxDoneCallbackFunc,
                                                       ( void * ) 0, &timerId ) ) != SOLCLIENT_OK ) {
                common_handleError ( rc, "solClient_context_startTimer()" );
                goto sessionCreated;
            }
        } else {
            /* In sub mode only. */
            printf ( "Waiting to receive %u message(s) or more ... \n", msgNum_s );
        }

        /*
         * Now wait for the message receive to finish receiving all messages
         * if using a subscriber. Simple polling is used here, but some sort
         * of thread syncrhonization object could be used. 
         */
        while ( !rxDone_s && !exitEarly_s && !rxTimeout_s ) {
            sleepInUs ( 100000 );   /* Check every 100 ms. */
        }
        if ( !rxTimeout_s && usePub_s ) {
            if ( ( rc = solClient_context_stopTimer ( contextThreadInfo.context_p, &timerId ) ) != SOLCLIENT_OK ) {
                common_handleError ( rc, "solClient_context_stopTimer()" );
                goto sessionCreated;
            }
        }
    }

    endTime = getTimeInUs (  );
    getUsageTime ( &userTime, &sysTime );
    elapsedTime = endTime - startTime;
    printf ( "\nElasped time: %lld us, user time: %lld us, sys time: %lld us\n"
             "Percent CPU: %Lf\n", elapsedTime, userTime, sysTime,
             ( long double ) 100.0 *
             ( ( ( long double ) userTime + ( long double ) sysTime ) / ( long double ) elapsedTime ) );

    printStats ( session_p );

    /************* Cleanup *************/

sessionConnected:
    /* Disconnect the Session. */
    if ( ( rc = solClient_session_disconnect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_disconnect()" );
    }

sessionCreated:
    /* Destroy the Session. */
    if ( ( rc = solClient_session_destroy ( &session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_destroy()" );
    }

contextThreadCreated:
    /* Stop the Context thread. */
    common_stopContextThread ( &contextThreadInfo );

contextCreated:
    /* Destroy the Context. */
    if ( ( rc = solClient_context_destroy ( &contextThreadInfo.context_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_context_destroy()" );
    }

initialized:
    /* Cleanup solClient. */
    if ( ( rc = solClient_cleanup (  ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_cleanup()" );
    }
    goto notInitialized;

notInitialized:
    /* Nothing to do - just exit. */

    return 0;
}

#if defined(__cplusplus)
}
#endif /* __cplusplus */
