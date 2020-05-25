
/** @example ex/secureSession.c 
 */

/*
 * This sample demonstrates:
 *  - Subscribing to a Topic for Direct messages.
 *  - Publishing Direct messages to a Topic.
 *  - Receiving messages through a callback function.
 *  - Explicitly configure session encryption properties
 *  - Connecting a session to the appliance using SSL over TCP
 *
 * This sample shows the basics of creating a Context, creating a Secure
 * Session, connecting a Session using SSL over TCP, subscribing to a Topic,
 * and publishing Direct messages to a Topic. It uses a message callback that 
 * simply prints any received message to the screen.
 *
 * A server certificate needs to be installed on the appliance and SSL must be
 * enabled on the appliance for this sample to work.
 * Also, in order to connect to the appliance with Certificate Validation enabled
 * (which is enabled by default), the appliance's certificate chain must be signed
 * by one of the root CAs in the trust store used by the sample.
 *
 * For this sample to use CLIENT CERTIFICATE authentication, a trust store has to
 * be set up on the appliance and it must contain the root CA that signed the client
 * certificate. The VPN must also have client-certificate authentication enabled.
 *
 * Copyright 2009-2020 Solace Corporation. All rights reserved.
 */

/*****************************************************************************
    For Windows builds, os.h should always be included first to ensure that
    _WIN32_WINNT is defined before winsock2.h or windows.h get included.
 *****************************************************************************/
#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"

#include "common.h"
typedef enum _authenticationScheme {
  _AUTHENTICATION_SCHEME_BASIC,
  _AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE,
} _authenticationScheme_t; 

/**
 * @struct options
 * The structure used to store options. Most of these options are
 * parsed from the command line.
 */
struct options
{
    char            targetHost[256];
    char            username[SOLCLIENT_SESSION_PROP_MAX_USERNAME_LEN + 1];
    char            password[SOLCLIENT_SESSION_PROP_MAX_PASSWORD_LEN + 1];
    char            vpn[SOLCLIENT_SESSION_PROP_MAX_VPN_NAME_LEN + 1];
    int             numMsgsToSend;
    char            *sslTrustStoreDir_p;
    char            *sslCommonNames_p;
    char            *sslExcludedProtocols_p;
    char            *sslCipherList_p;
    BOOL            isCertificateVarificationOff; 
    BOOL            isCertificateDateVarificationOff; 
    char            *sslCertFile_p;
    char            *sslKeyFile_p;
    char            *sslKeyPasswd_p;
    _authenticationScheme_t   authScheme;
    char            *sslDowngrade_p;
    solClient_log_level_t logLevel;
};


#define USAGE_PARAMS      \
        "\t-c, --cip=tcps:ip[:port] protocol, IP and port of the messaging appliance (e.g. --cip=tcps:192.168.160.101).\n"\
        "\t-u, --cu=[user][@vpn] Client username and Mesage VPN name. The VPN name is optional and\n"\
        "\t                only used in a Solace messaging appliance running SolOS-TR.\n"\
        "\t-p, --cp=password Client password.\n"\
        "\t-T  --dir=directory Full directory path name where the trusted certificates are.\n"\
        "\t                It is required if the certificate verification is enabled.\n"			\
        "\t-N  --cn=commonnames List of comma separated trusted common names.\n"\
        "\t-C  --cipher=ciphers List of comma separated cipher suites.\n"\
        "\t-P  --prot=list of excluded SSL protocols, separated by comma.\n"\
        "\t-E  --cert=certFile Client certificate file name.\n"\
        "\t-Y  --key=keyFile Client certificate private key file name.\n"\
        "\t-W  --passwd=password Encrypted client certificate private key file password.\n"\
        "\t-k  --auth=authentication scheme: 0=basic, 1=client-certificate.\n"\
        "\t-i    certificate verification is disabled (enabled by default).\n"\
        "\t-j    certificate date verification is disabled (enabled by default).\n"\
        "\t-l, --log=loglevel  API and application logging level (debug, info, notice, warn, error, critical).\n" \
        "\t-d   -downgr=PLAIN_TEXT Downgrade SSL connection to 'PLAIN_TEXT' after client authentication.\n"

/*****************************************************************************
 * printUsage
 *
 * Prints the usage instructions for this example.
 *****************************************************************************/
static void
printUsage (  )
{
    printf ( "\nUsage: secureSession PARAMETERS\n\n" "Where PARAMETERS are: \n" USAGE_PARAMS );
}

/*****************************************************************************
 * messageReceiveCallback
 *
 * The message callback is invoked for each Direct message received by
 * the Session. In this sample, the message is printed to the screen.
 * 
 * Message callback code is executed within the API thread, which means that
 * it should deal with the message quickly, or queue the message for further
 * processing in another thread.
 * 
 * Note: In other samples, a common message handler is used. However, to
 * emphasize this programming paradigm, this sample direcly includes the message
 * receive callback.
 *****************************************************************************/
solClient_rxMsgCallback_returnCode_t
messageReceiveCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_returnCode_t rc = SOLCLIENT_OK;

    printf ( "Received message:\n" );
    if ( ( rc = solClient_msg_dump ( msg_p, NULL, 0 ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_msg_dump()" );
        return SOLCLIENT_CALLBACK_OK;
    }

    printf ( "\n" );

    /* 
     * Returning SOLCLIENT_CALLBACK_OK causes the API to free the memory 
     * used by the message. This is important to avoid leaks in applications
     * that do not intend to manage the release of each message's memory.
     *
     * If the message callback is being used to queue the messages for
     * further processing, it should return SOLCLIENT_CALLBACK_TAKE_MSG. In
     * this case, it becomes the responsibility of the application to free
     * the memory.
     */
    return SOLCLIENT_CALLBACK_OK;
}

/*****************************************************************************
 * parseCommandOptions
 *****************************************************************************/
static int
parseCommandOptions ( int argc, charPtr32 *argv, struct options *opt )
{
    static char    *optstring = "c:l:n:p:u:P:C:N:T:ijE:Y:W:k:d:";
    static struct option longopts[] = {
        {"cip", 1, NULL, 'c'},
        {"cu", 1, NULL, 'u'},
        {"mn", 1, NULL, 'n'},
        {"log", 1, NULL, 'l'},
        {"cp", 1, NULL, 'p'},
        {"dir", 1, NULL, 'T'},
        {"cn", 1, NULL, 'N'},
        {"cipher", 1, NULL, 'C'},
        {"prot", 1, NULL, 'P'},
        {"cert", 1, NULL, 'E'},
        {"key", 1, NULL, 'Y'},
        {"passwd", 1, NULL, 'W'},
        {"auth", 1, NULL, 'k'},
        {"downgr", 1, NULL, 'd'},
        {0, 0, 0, 0}
    };
    int             c;
    int             rc = 1;
    char           *end_p;
    size_t            strLen;
    char           *begin_p;
    charPtr        *argVOS;
    opt->username[0] = ( char ) 0;
    opt->password[0] = ( char ) 0;
    opt->vpn[0] = ( char ) 0;
    opt->targetHost[0] = ( char ) 0;
    opt->numMsgsToSend = 10;
    opt->logLevel = SOLCLIENT_LOG_DEFAULT_FILTER;
    opt->sslTrustStoreDir_p = NULL;
    opt->sslCommonNames_p = NULL;
    opt->sslExcludedProtocols_p = NULL;
    opt->sslCipherList_p = NULL;
    opt->isCertificateVarificationOff = FALSE; 
    opt->isCertificateDateVarificationOff = FALSE;
    opt->sslCertFile_p = NULL;
    opt->sslKeyFile_p = NULL;
    opt->sslKeyPasswd_p = NULL;
    opt->authScheme = _AUTHENTICATION_SCHEME_BASIC;
    opt->sslDowngrade_p = NULL;

    INIT_OS_ARGS(argc, argv, argVOS);
    while ( ( c = getopt_long ( argc, argVOS, optstring, longopts, NULL ) ) != -1 ) {
        switch ( c ) {
            case 'c':
                strncpy ( opt->targetHost, optarg, sizeof ( opt->targetHost ) );
                begin_p = opt->targetHost;
                strLen = (int)strlen(begin_p);
                end_p = begin_p + strLen;
                while (begin_p < end_p) {
                    if ((strncasecmp( begin_p, "tcps:", 5) != 0) &&
                        (strncasecmp( begin_p, "wss:", 4) != 0) &&
                        (strncasecmp( begin_p, "https:", 6) != 0)) {
                        printf("%s: support secure transport protocols only\n", opt->targetHost);
                        return 0;
                    }
                    begin_p = strchr(begin_p, ',');
                    if (begin_p == NULL) {
                      break; /* no separator ',' found */
                    }
                    begin_p++; /* skip the separator */
                }
                break;

            case 'l':
                opt->logLevel = ( solClient_log_level_t ) strtol ( optarg, &end_p, 0 );
                if ( ( opt->logLevel > SOLCLIENT_LOG_DEBUG ) || ( *end_p != ( char ) 0 ) ) {
                    if ( strcasecmp ( optarg, "debug" ) == 0 ) {
                        opt->logLevel = SOLCLIENT_LOG_DEBUG;
                    } else if ( strcasecmp ( optarg, "info" ) == 0 ) {
                        opt->logLevel = SOLCLIENT_LOG_INFO;
                    } else if ( strcasecmp ( optarg, "notice" ) == 0 ) {
                        opt->logLevel = SOLCLIENT_LOG_NOTICE;
                    } else if ( strcasecmp ( optarg, "warn" ) == 0 ) {
                        opt->logLevel = SOLCLIENT_LOG_WARNING;
                    } else if ( strcasecmp ( optarg, "error" ) == 0 ) {
                        opt->logLevel = SOLCLIENT_LOG_ERROR;
                    } else if ( strcasecmp ( optarg, "critical" ) == 0 ) {
                        opt->logLevel = SOLCLIENT_LOG_CRITICAL;
                    } else {
                        rc = 0;
                    }
                }
                break;
            case 'n':
                opt->numMsgsToSend = atoi ( optarg );
                if ( opt->numMsgsToSend <= 0 )
                    rc = 0;
                break;
            case 'u':
                common_parseUsernameAndVpn ( optarg,
                                             opt->username,
                                             sizeof ( opt->username ), opt->vpn, sizeof ( opt->vpn ) );
                break;
            case 'p':
                strncpy ( opt->password, optarg, sizeof ( opt->password ) );
                break;
            case 'T':
                strLen = strlen(optarg);
                opt->sslTrustStoreDir_p = (char *)malloc(strLen+1);
                if (opt->sslTrustStoreDir_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslTrustStoreDir_p, optarg, strLen +1 );
                break;
            case 'N':
                strLen = strlen(optarg);
                opt->sslCommonNames_p = (char *)malloc(strLen+1);
                if (opt->sslCommonNames_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslCommonNames_p, optarg, strLen +1 );
                break;
            case 'C':
                strLen = strlen(optarg);
                opt->sslCipherList_p = (char *)malloc(strLen+1);
                if (opt->sslCipherList_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslCipherList_p, optarg, strLen +1 );
                break;
            case 'P':
                strLen = strlen(optarg);
                opt->sslExcludedProtocols_p = (char *)malloc(strLen+1);
                if (opt->sslExcludedProtocols_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslExcludedProtocols_p, optarg, strLen +1 );
                break;
            case 'i':
                opt->isCertificateVarificationOff = TRUE;
                break;
            case 'j':
                opt->isCertificateDateVarificationOff = TRUE;
                break;
            case 'E':
                strLen = strlen(optarg);
                opt->sslCertFile_p = (char *)malloc(strLen+1);
                if (opt->sslCertFile_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslCertFile_p, optarg, strLen +1 );
                break;
            case 'Y':
                strLen = strlen(optarg);
                opt->sslKeyFile_p = (char *)malloc(strLen+1);
                if (opt->sslKeyFile_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslKeyFile_p, optarg, strLen +1 );
                break;
            case 'W':
                strLen = strlen(optarg);
                opt->sslKeyPasswd_p = (char *)malloc(strLen+1);
                if (opt->sslKeyPasswd_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslKeyPasswd_p, optarg, strLen +1 );
                break;
            case 'k':
                opt->authScheme = ( _authenticationScheme_t ) strtol ( optarg, &end_p, 0 );
                if ( ( opt->authScheme > _AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE) || ( *end_p != ( char ) 0 ) ) {
                    if ( strcasecmp ( optarg, "basic" ) == 0 ) {
                        opt->authScheme = _AUTHENTICATION_SCHEME_BASIC;
                    } else if ( strcasecmp ( optarg, "client-certificate" ) == 0 ) {
                        opt->authScheme = _AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE;
                    } else {
                        rc = 0;
                    }
                }
                break;
            case 'd':
                strLen = strlen(optarg);
                opt->sslDowngrade_p = (char *)malloc(strLen+1);
                if (opt->sslDowngrade_p == NULL) {
                    solClient_log ( SOLCLIENT_LOG_ERROR, "malloc failed: out of memory!"); 
                    exit (-1);
                }
                strncpy ( opt->sslDowngrade_p, optarg, strLen +1 );
                break;
            default:
                rc = 0;
                break;
        }
    }

    if ((!opt->authScheme == _AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE) &&
        (strlen(opt->username) == 0)) {
        printf ( "Missing required parameter '--cu'\n" );
        rc = 0;
    }

    if ((opt->isCertificateVarificationOff == FALSE) &&
             (opt->sslTrustStoreDir_p == NULL)) {
        printf ( "Missing required parameter '--dir'\n" );
        rc = 0;
    }
    if ((opt->authScheme == _AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE) && 
        ((opt->sslKeyFile_p == NULL) ||
         (opt->sslCertFile_p == NULL))) {
        printf ( "Missing required parameters '--cert' and/or '--key'\n" );
        rc = 0;
    }
    FREE_OS_ARGS
    return ( rc );
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
    struct options commandOpts;

    /* Context */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session */
    solClient_opaqueSession_pt session_p;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    /* Session Properties */
    const char     *sessionProps[60];
    int             propIndex = 0;

    /* Message */
    solClient_opaqueMsg_pt msg_p = NULL;
    solClient_destination_t destination;
    int             msgsSent = 0;


    printf ( "\nsecureSession.c (Copyright 2009-2020 Solace Corporation. All rights reserved.)\n" );

    /*************************************************************************
     * Parse command options
     *************************************************************************/
    if ( parseCommandOptions ( argc, argv, &commandOpts ) == 0 ) {
        goto printUsage;
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
     * application. In this case, the ALL category is used to set the log level for 
     * both at the same time.
     */
    solClient_log_setFilterLevel ( SOLCLIENT_LOG_CATEGORY_ALL, commandOpts.logLevel );

    /*************************************************************************
     * Create a Context
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

    solClient_log ( SOLCLIENT_LOG_INFO, "Creating solClient session." );

    /*
     * Note: In other samples, common functions have been used to create 
     * and connect Sessions. However, for demonstration purposes, this sample
     * includes Session creation and connection in line.
     */

    /* Configure the Session function information. */
    sessionFuncInfo.rxMsgInfo.callback_p = messageReceiveCallback;
    sessionFuncInfo.rxMsgInfo.user_p = NULL;
    sessionFuncInfo.eventInfo.callback_p = common_eventCallback;
    sessionFuncInfo.eventInfo.user_p = NULL;

    /* Configure the Session properties. */
    propIndex = 0;

    if ( commandOpts.targetHost[0] != (char) 0 ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = commandOpts.targetHost;
    }

    if (commandOpts.authScheme ==  _AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME;
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE;


        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_CLIENT_CERTIFICATE_FILE;
        sessionProps[propIndex++] = commandOpts.sslCertFile_p;

        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_CLIENT_PRIVATE_KEY_FILE;
        sessionProps[propIndex++] = commandOpts.sslKeyFile_p;

        if (commandOpts.sslKeyPasswd_p != NULL) {
            sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_CLIENT_PRIVATE_KEY_FILE_PASSWORD;
            sessionProps[propIndex++] = commandOpts.sslKeyPasswd_p;
        }
    }

    if (strlen(commandOpts.username)>0) {
         sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
         sessionProps[propIndex++] = commandOpts.username;
    
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
        sessionProps[propIndex++] = commandOpts.password;
    }

    if ( commandOpts.vpn[0] ) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
        sessionProps[propIndex++] = commandOpts.vpn;
    }

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES;
    sessionProps[propIndex++] = "3";

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_CONNECT_RETRIES_PER_HOST;
    sessionProps[propIndex++] = "3";

    /*
     * Note: Reapplying subscriptions allows Sessions to reconnect after
     * failure and have all their subscriptions automatically restored. For
     * Sessions with many subscriptions this can increase the amount of time
     * required for a successful reconnect.
     */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    if (commandOpts.isCertificateVarificationOff == TRUE) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_VALIDATE_CERTIFICATE;
        sessionProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
    }
    if (commandOpts.isCertificateDateVarificationOff == TRUE) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_VALIDATE_CERTIFICATE_DATE;
        sessionProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
    }


    if (commandOpts.sslTrustStoreDir_p != NULL) {
      sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_TRUST_STORE_DIR;
      sessionProps[propIndex++] = commandOpts.sslTrustStoreDir_p;
    }

    if (commandOpts.sslCipherList_p != NULL) {
      sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_CIPHER_SUITES;
      sessionProps[propIndex++] = commandOpts.sslCipherList_p;
    }
    if (commandOpts.sslCommonNames_p != NULL) {
      sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_TRUSTED_COMMON_NAME_LIST;
      sessionProps[propIndex++] = commandOpts.sslCommonNames_p;
    }
    if (commandOpts.sslExcludedProtocols_p != NULL) {
      sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_EXCLUDED_PROTOCOLS;
      sessionProps[propIndex++] = commandOpts.sslExcludedProtocols_p;
    }
    if (commandOpts.sslDowngrade_p != NULL) {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_CONNECTION_DOWNGRADE_TO;
        sessionProps[propIndex++] = commandOpts.sslDowngrade_p;
    }
    sessionProps[propIndex] = NULL;

    /* Create the Session. */
    if ( ( rc = solClient_session_create ( sessionProps,
                                           context_p,
                                           &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_create()" );
        goto cleanup;
    }

    /* Connect the Session. */
    if ( ( rc = solClient_session_connect ( session_p ) ) != SOLCLIENT_OK ) {
        common_handleError ( rc, "solClient_session_connect()" );
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

    for ( msgsSent = 0; msgsSent < commandOpts.numMsgsToSend; ++msgsSent ) {

        /* Allocate memory for the message that is to be sent. */
        if ( ( rc = solClient_msg_alloc ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_alloc()" );
            goto sessionConnected;
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

        /* Add some content to the message. */
        if ( ( rc = solClient_msg_setBinaryAttachment ( msg_p,
                                                        COMMON_ATTACHMENT_TEXT,
                                                        ( solClient_uint32_t ) strlen ( COMMON_ATTACHMENT_TEXT ) ) ) !=
             SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_setBinaryAttachment()" );
            goto freeMessage;
        }

        /* Send the message. */
        if ( ( rc = solClient_session_sendMsg ( session_p, msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_session_sendMsg()" );
            goto freeMessage;
        }

      freeMessage:
        if ( ( rc = solClient_msg_free ( &msg_p ) ) != SOLCLIENT_OK ) {
            common_handleError ( rc, "solClient_msg_free()" );
            goto sessionConnected;
        }

        /* 
         * Wait one second between sending messages. This provides time for 
         * the final message to be received.
         */
        sleepInSec ( 1 );
    }

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
    goto notInitialized;

  printUsage:
    printUsage (  );

  notInitialized:
    /* free resourse */
    if (commandOpts.sslTrustStoreDir_p != NULL) {
      free((void *)commandOpts.sslTrustStoreDir_p);
    }
    if (commandOpts.sslCipherList_p != NULL) {
      free((void *)commandOpts.sslCipherList_p);
    }
    if (commandOpts.sslCommonNames_p != NULL) {
      free((void *)commandOpts.sslCommonNames_p);
    }
    if (commandOpts.sslExcludedProtocols_p != NULL) {
      free((void *)commandOpts.sslExcludedProtocols_p);
    }   
    if (commandOpts.sslCertFile_p != NULL) {
      free((void *)commandOpts.sslCertFile_p);
    }   
    if (commandOpts.sslKeyFile_p != NULL) {
      free((void *)commandOpts.sslKeyFile_p);
    }
    if (commandOpts.sslKeyPasswd_p != NULL) {
      free((void *)commandOpts.sslKeyPasswd_p);
    }
    if (commandOpts.sslDowngrade_p != NULL) {
        free((void *)commandOpts.sslDowngrade_p);
    }

    return 0;
}
