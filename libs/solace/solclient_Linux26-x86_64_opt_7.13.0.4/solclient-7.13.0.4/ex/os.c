
/* Copyright 2007-2020 Solace Corporation. All rights reserved. */

#include "os.h"

#ifdef WIN32
#else
// #include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/resource.h>
#	if defined(DARWIN_OS)
#include <mach/clock.h>
#include <mach/mach.h>
#	endif
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#       if defined (SOLCLIENT_ZTPF_BUILD)
#include <tpf/tpfapi.h>
#       endif

/* Semaphore that will be posted to when CTRL-C is hit. */
SEM_T           ctlCSem;
BOOL            gotCtlC = FALSE;

/* 
 * Signal handler -- posts to ctlCSem 
 * If we already got a CTRL-C, then exit immediately.
 */
#ifdef WIN32

#define strncasecmp (_strnicmp)

BOOL
sigHandler ( DWORD ctlType )
{
    BOOL            rc = FALSE; /* signal not handled */

    if ( ctlType == CTRL_C_EVENT ) {
        if ( !gotCtlC ) {
            gotCtlC = TRUE;
            semPost ( &ctlCSem );
            rc = TRUE;
        } else {
            exit ( 0 );
        }
    }
    return rc;
}
#else
static void
sigHandler ( int sigNum )
{
    if ( sigNum == SIGINT ) {
        if ( !gotCtlC ) {
            gotCtlC = TRUE;
            semPost ( &ctlCSem );
        } else {
            exit ( 0 );
        }
    }
}
#endif

void
initSigHandler ( void )
{
    semInit ( &ctlCSem, 0, 1 );
#ifdef WIN32
    if ( SetConsoleCtrlHandler ( ( PHANDLER_ROUTINE ) sigHandler, TRUE /* add */  ) == 0 ) {
        printf ( "Could not initialize Control C handler\n" );
    }
#else
    if ( signal ( SIGINT, sigHandler ) == SIG_ERR ) {
        printf ( "Could not initialize Control C handler\n" );
    }
#endif
}

/* argv bit 32-64 abstraction */
int getArgVOS(int argc, charPtr32* argv, ptr* argVOS)
{
#if defined(__VMS) && __INITIAL_POINTER_SIZE==64
    int i=0;
    charPtr* __argVOS = 0;
    __argVOS = (charPtr*)_malloc64(argc * sizeof(charPtr));
    charPtr32* tmp = (charPtr32*)argv;
    for(i=0;i<argc;i++)
    {
        __argVOS[i] = *tmp;
        tmp++;
    }

    (*argVOS) = (ptr)__argVOS;
    return 1;
#else
    return 0;
#endif
}

void
sleepInSec ( int secToSleep )
{
    sleepInUs ( secToSleep * 1000000 );
}

void
sleepInUs ( int usToSleep )
{
#ifdef WIN32
    DWORD           millis = ( DWORD ) ( usToSleep / 1000 );
    DWORD           extra = ( DWORD ) ( usToSleep % 1000 );
    if ( extra > 0 ) {
        millis++;
    }

    Sleep ( millis );
#else
    struct timespec timeVal;
    struct timespec timeRem;

    time_t tv_sec = usToSleep / 1000000;
    /*
     * z/TPF at least does not support non-zero seconds, loop
     * over the seconds. We lose 1 ns on every loop, big whoop
     */
    do {
        timeVal.tv_sec = 0;
        if (tv_sec) {
            timeVal.tv_nsec = 999999999;
        } else {
            timeVal.tv_nsec = (usToSleep % 1000000) * 1000;
        }
again:
        if (nanosleep(&timeVal, &timeRem) < 0) {
            if ( (errno == EINTR) && !gotCtlC ) {
                /* Nanosleep was interrupted, probably by our sig alarm */
                timeVal = timeRem;
                goto again;
            }
            else {
                break;
            }
        }
    } while (tv_sec--);
#endif
}


UINT64
getTimeInUs ( void )
{

#ifdef WIN32

    FILETIME        fileTime;
    LARGE_INTEGER   theTime;

    /* Gets time in 100 nanosecond intervals */
    GetSystemTimeAsFileTime ( &fileTime );
    theTime.LowPart = fileTime.dwLowDateTime;
    theTime.HighPart = fileTime.dwHighDateTime;
    return theTime.QuadPart / 10;       /* convert to microseconds */
#else
	struct timespec tv;
#	if defined(DARWIN_OS)
    
	// timespec uses long; mach timespec uses unsigned int
    mach_timespec_t mtv;
    clock_serv_t cclock;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mtv);
    mach_port_deallocate(mach_task_self(), cclock);

	tv.tv_sec = mtv.tv_sec;
	tv.tv_nsec = mtv.tv_nsec;
	
#	else
    clock_gettime ( CLOCK_REALTIME, &tv );
#	endif
    return ( ( UINT64 ) tv.tv_sec * ( UINT64 ) 1000000 ) + ( ( UINT64 ) tv.tv_nsec / ( UINT64 ) 1000 );

#endif

}

void
_getDateTime ( char *buf_p, int bufSize )
{

#ifdef WIN32
    SYSTEMTIME      sysTime;
    char           *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    char           *months[] = { "x", "Jan", "Feb", "Mar", "Apr", "May",
        "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    GetLocalTime ( &sysTime );
    /* Hmmm, cannot seem to get built-in formatting methods to work, so do it ourselves */
    snprintf ( buf_p, bufSize, "%s %s %2d %02d:%02d:%02d.%03ld %d",
               days[sysTime.wDayOfWeek], months[sysTime.wMonth], sysTime.wDay,
               sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, sysTime.wYear );
#else
    struct timeval  tv;
    struct tm       timeStruct;
    char            buffer[80]; /* temp staging buffer */

    gettimeofday ( &tv, NULL );
    localtime_r ( &tv.tv_sec, &timeStruct );
    strftime ( buffer, sizeof ( buffer ), "%a %b %d %T", &timeStruct );
    snprintf ( buf_p, bufSize, "%s.%03ld %d", buffer, ((long) tv.tv_usec + 500 ) / 1000, 1900 + timeStruct.tm_year );
#endif

}

/*
 * Also returns the amount of time waited.
 */
UINT64
waitUntil ( UINT64 nexttimeInUs )
{
    UINT64          currTime = getTimeInUs (  );
    UINT64          waitTime;
#ifdef WIN32
    UINT64          waitTimeInMSec;
#endif

    if ( currTime > nexttimeInUs ) {
        /* We're behind so just return. */
        return 0;
    }
    waitTime = nexttimeInUs - currTime;


#ifdef WIN32
    waitTimeInMSec = waitTime / 1000;
    Sleep ( ( DWORD ) waitTimeInMSec );
#elif defined (SOLCLIENT_ZTPF_BUILD)
     dlayc();
#else
    struct timeval  tv;
    tv.tv_sec = 0;
    tv.tv_usec = (int)waitTime;
    select ( 0, 0, 0, 0, &tv );
#endif
    return waitTime;
}

UINT64
getCpuUsageInUs (  )
{
#if ! defined (WIN32) && ! defined (__VMS) && ! defined (SOLCLIENT_ZTPF_BUILD) 
    struct rusage   resUsage;
    memset ( &resUsage, 0, sizeof ( resUsage ) );
    int             retval = getrusage ( RUSAGE_SELF, &resUsage );
    if ( retval != 0 )
        perror ( "getrusage" );
    UINT64          usage =
            ( UINT64 ) ( resUsage.ru_utime.tv_sec ) * 1000000LL +
            ( UINT64 ) ( resUsage.ru_utime.tv_usec ) +
            ( UINT64 ) ( resUsage.ru_stime.tv_sec ) * 1000000LL + ( UINT64 ) ( resUsage.ru_stime.tv_usec );
    return usage;
#else
    return 0;
#endif
}

void
getUsageTime ( long long *userTime_p, long long *systemTime_p )
{
#if defined( __VMS) || defined (SOLCLIENT_ZTPF_BUILD)
*userTime_p = 0;
*systemTime_p = 0;
#else
#ifdef WIN32
    FILETIME        creationTime;
    FILETIME        exitTime;
    FILETIME        kernelTime;
    FILETIME        userTime;
    LARGE_INTEGER   theTime;

    GetProcessTimes ( GetCurrentProcess (  ), &creationTime, &exitTime, &kernelTime, &userTime );
    /* File times are in units of 100 ns */
    theTime.LowPart = userTime.dwLowDateTime;
    theTime.HighPart = userTime.dwHighDateTime;
    *userTime_p = theTime.QuadPart / 10;        /* convert to microseconds */
    theTime.LowPart = kernelTime.dwLowDateTime;
    theTime.HighPart = kernelTime.dwHighDateTime;
    *systemTime_p = theTime.QuadPart / 10;      /* convert to microseconds */
#else
    struct rusage   usage;

    getrusage ( RUSAGE_SELF, &usage );
    *userTime_p = ( long long ) usage.ru_utime.tv_sec * ( long long ) 1000000 + ( long long ) usage.ru_utime.tv_usec;
    *systemTime_p = ( long long ) usage.ru_stime.tv_sec * ( long long ) 1000000 + ( long long ) usage.ru_stime.tv_usec;
#endif
#endif
}


BOOL
mutexInit ( MUTEX_T * mutex_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    *mutex_p = CreateMutex ( NULL, FALSE, NULL );
    if ( *mutex_p == NULL ) {
        /* Unable to create Mutex */
        rc = FALSE;
    }
#else
    int             osRc = pthread_mutex_init ( ( MUTEX_T * ) mutex_p, NULL );
    if ( osRc != 0 ) {
        /* pthread does NOT set errno, but returns the equivalent value */
        rc = FALSE;
    }
#endif
    return rc;
}

BOOL
mutexDestroy ( MUTEX_T * mutex_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    if ( ( *mutex_p != NULL ) && ( !CloseHandle ( *mutex_p ) ) ) {
        /* Could not destroy mutex */
        rc = FALSE;
    }
#else
    int             osRc = pthread_mutex_destroy ( ( MUTEX_T * ) mutex_p );
    if ( osRc != 0 ) {
        /* pthread does NOT set errno, but returns the equivalent value */
        rc = FALSE;
    }
#endif
    return rc;
}

BOOL
mutexLock ( MUTEX_T * mutex_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    DWORD           waitResult = WaitForSingleObject ( *mutex_p, INFINITE );
    if ( waitResult != WAIT_OBJECT_0 ) {
        /* Could not lock  mutex, result */
        rc = FALSE;
    }
#else
    int             osRc = pthread_mutex_lock ( ( MUTEX_T * ) mutex_p );
    if ( osRc != 0 ) {
        /* pthread does NOT set errno, but returns the equivalent value */
        rc = FALSE;
    }
#endif
    return rc;
}

BOOL
mutexUnlock ( MUTEX_T * mutex_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    if ( !ReleaseMutex ( *mutex_p ) ) {
        /* Could not unlock mutex */
        rc = FALSE;
    }
#else
    int             osRc = pthread_mutex_unlock ( ( MUTEX_T * ) mutex_p );
    if ( osRc != 0 ) {
        /* pthread does NOT set errno, but returns the equivalent value */
        rc = FALSE;
    }
#endif
    return rc;

}

BOOL
condInit ( CONDITION_T * cond_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    *cond_p = CreateEvent ( NULL, TRUE, TRUE, NULL );
    if ( *cond_p == NULL ) {
        /* Could not create event for condition */
        rc = FALSE;
    } else {
        if ( !ResetEvent ( *cond_p ) ) {
            rc = FALSE;
        }
    }
#else
    int             osRc = pthread_cond_init ( cond_p, NULL );
    if ( osRc != 0 ) {
        /*
         * pthread does NOT set errno, but returns the equivalent value
         * Could not create event for condition 
         */
        rc = FALSE;
    }
#endif

    return rc;

}

BOOL
condWait ( CONDITION_T * cond_p, MUTEX_T * mutex_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    ( void ) SignalObjectAndWait ( *mutex_p, *cond_p, INFINITE, FALSE );
    rc = mutexLock ( mutex_p );
#else
    pthread_cond_wait ( cond_p, mutex_p );
#endif

    return rc;
}

BOOL
condReset ( CONDITION_T * cond_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    ResetEvent ( *cond_p );
#else
#endif

    return rc;
}

BOOL
condDestroy ( CONDITION_T * cond_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    if ( ( *cond_p != NULL ) && !CloseHandle ( *cond_p ) ) {
        /* Could not close condition event */
        rc = FALSE;
    }
#else
    int             osRc = pthread_cond_destroy ( cond_p );
    if ( osRc != 0 ) {
        /* 
         * pthread does NOT set errno, but returns the equivalent value
         * Could not close condition event 
         */
        rc = FALSE;
    }
#endif

    return rc;

}

BOOL
condTimedWait ( CONDITION_T * cond_p, MUTEX_T * mutex_p, int timeoutSec /* negative means forever */
         )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    DWORD           waitInterval;
    DWORD           waitResult;
    if ( timeoutSec < 0 ) {
        waitInterval = INFINITE;
    } else {
        waitInterval = ( DWORD ) timeoutSec *1000;      /* convert to ms */
    }

    waitResult = SignalObjectAndWait ( *cond_p, *mutex_p, waitInterval, FALSE );
    ( void ) mutexLock ( mutex_p );
    if ( waitResult == WAIT_TIMEOUT ) {
        /* Wait on condition timed out, timeout */
        return FALSE;
    } else if ( waitResult != WAIT_OBJECT_0 ) {
        /* Could not wait on condition, result = */
        rc = FALSE;
    }
#else
    int             osRc;

    if ( timeoutSec < 0 ) {
        osRc = pthread_cond_wait ( cond_p, mutex_p );
    } else {
		struct timespec absTimeout;
#	ifdef DARWIN_OS
		mach_timespec_t mtv;
		clock_serv_t cclock;
		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mtv);
		mach_port_deallocate(mach_task_self(), cclock);
		
		absTimeout.tv_sec = mtv.tv_sec;
		absTimeout.tv_nsec = mtv.tv_nsec;
#	else
        osRc = clock_gettime ( CLOCK_REALTIME, &absTimeout );
        if ( osRc < 0 ) {
            /* Could not get time for condition wait, error = */
            return FALSE;
        }
#	endif
        absTimeout.tv_sec += timeoutSec;
        osRc = pthread_cond_timedwait ( cond_p, mutex_p, &absTimeout );
    }
    if ( osRc != 0 ) {
        /* pthread does NOT set errno, but returns the equivalent value */
        if ( osRc == ETIMEDOUT ) {
            /* Wait on condition timed out, timeout = %d sec */
            return FALSE;
        }
        /* Could not wait on condition, error = %d */
        rc = FALSE;
    }
#endif
    return rc;

}

BOOL
condSignal ( CONDITION_T * cond_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    if ( !SetEvent ( *cond_p ) ) {
        /* Could not signal condition */
        rc = FALSE;
    }
#else
    int             osRc = pthread_cond_signal ( cond_p );
    if ( osRc != 0 ) {
        /*
         * pthread does NOT set errno, but returns the equivalent value
         * Could not signal condition, error = %d
         */
        rc = FALSE;
    }
#endif
    return rc;

}

BOOL
semInit ( SEM_T * sem_p, unsigned int initValue, unsigned int maxValue )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    *sem_p = CreateSemaphore ( NULL, ( LONG ) initValue, ( LONG ) maxValue, NULL );
    if ( *sem_p == NULL ) {
        rc = FALSE;
    }
#elif defined(DARWIN_OS)
	snprintf(sem_p->name, 9, "%x", rand());
	if ((sem_p->semaphore = sem_open(sem_p->name, O_CREAT, 0644, initValue)) == SEM_FAILED) {
		rc = FALSE;
	}
#else
    if ( sem_init ( sem_p, 0, initValue ) != 0 ) {
        rc = FALSE;
    }
#endif
    return rc;
}

BOOL
semDestroy ( SEM_T * sem_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    if ( ( *sem_p != NULL ) && ( !CloseHandle ( *sem_p ) ) ) {
        rc = FALSE;
    }
#elif defined(DARWIN_OS)
	if (sem_close(sem_p->semaphore) == -1) {
		rc = FALSE;
	}
	if (sem_unlink(sem_p->name) == -1) {
		rc = FALSE;
	}
#else
    if ( sem_destroy ( sem_p ) != 0 ) {
        rc = FALSE;
    }
#endif
    return rc;
}

BOOL
semWait ( SEM_T * sem_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    if ( WaitForSingleObject ( *sem_p, INFINITE ) != WAIT_OBJECT_0 ) {
        rc = FALSE;
    }
#else
    int             osRc;
  tryAgain:
#	if defined(DARWIN_OS)
    osRc = sem_wait ( sem_p->semaphore );
#	else
    osRc = sem_wait ( sem_p );
#	endif
    if ( osRc != 0 ) {
        if ( errno == EINTR )
            goto tryAgain;
        rc = FALSE;
    }
#endif
    return rc;
}

BOOL
semPost ( SEM_T * sem_p )
{
    BOOL            rc = TRUE;

#ifdef WIN32
    LONG            previousCount;
    if ( !ReleaseSemaphore ( *sem_p, 1, &previousCount ) ) {
        rc = FALSE;
    }
#else
#	if defined(DARWIN_OS)
    if ( sem_post ( sem_p->semaphore ) != 0 ) {
#	else
	if ( sem_post ( sem_p ) != 0 ) {
#	endif
        rc = FALSE;
    }
#endif
    return rc;
}

THREAD_HANDLE_T
startThread ( FP fp, void *arg )
{
    THREAD_HANDLE_T th = _NULL_THREAD_ID;

#ifdef WIN32
    DWORD           thread_id;
    th = CreateThread ( NULL,   /* default security attributes */
                        0,      /* use default stack size */
                        ( LPTHREAD_START_ROUTINE ) fp,  /* thread function */
                        arg,    /* argument to thread function */
                        0,      /* use default creation flags */
                        &thread_id );   /* return thread identifier */
    if ( th == NULL ) {
        return _NULL_THREAD_ID;
    } else {
        return th;
    }
#else
    if ( pthread_create ( &th, NULL, fp, arg ) ) {
    }
    if ( th == _NULL_THREAD_ID ) {
        return _NULL_THREAD_ID;
    } else {
        return th;
    }
#endif
}

void
waitOnThread ( THREAD_HANDLE_T handle )
{
#ifdef WIN32
    WaitForSingleObject ( handle, INFINITE );
    CloseHandle ( handle );
    handle = NULL;
#else
    void           *value_p;
    pthread_join ( handle, &value_p );
#endif
}
