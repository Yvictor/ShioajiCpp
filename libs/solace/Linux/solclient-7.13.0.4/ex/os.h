
/** @example ex/os.h
 */

/* 
 * OS-specific methods for abstracting.  
 * Copyright 2008-2020 Solace Corporation. All rights reserved. 
 */

#ifndef ___OS_H_
#define ___OS_H_

#ifdef __cplusplus
extern          "C"
{
#endif
    typedef int     BOOL;
#define FALSE 0
#define TRUE 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* must define types before include getopt.h */
#ifdef __VMS
#pragma pointer_size save
#pragma pointer_size 32
    typedef char* charPtr32;
    typedef void* ptr32;
#pragma pointer_size 64
    typedef char* charPtr64;
    typedef void* ptr64;
#pragma pointer_size restore

#if __INITIAL_POINTER_SIZE==64
    typedef charPtr64 charPtr;
    typedef ptr64 ptr;
#define INIT_OS_ARGS(argc, argv, argVOS)            \
    static int __free_os_args;                      \
    static charPtr* __argVOS_g;                     \
    __free_os_args = getArgVOS(argc, argv, (ptr*)&argVOS); \
    __argVOS_g = argVOS
#else
    typedef charPtr32 charPtr;
    typedef ptr32 ptr;
#define INIT_OS_ARGS(argc, argv, argVOS)            \
    static int __free_os_args=0;                    \
    static charPtr* __argVOS_g=NULL;                \
    argVOS = argv
#endif

#define FREE_OS_ARGS        \
    if(__free_os_args)      \
    {                       \
        free(__argVOS_g);   \
    }
#else
    typedef void* ptr;
    typedef ptr ptr32;
    typedef ptr ptr64;
    typedef char* charPtr;
    typedef charPtr charPtr32;
    typedef charPtr charPtr64;
#define INIT_OS_ARGS(argc, argv, argVOS) argVOS=argv
#define FREE_OS_ARGS
#endif


#ifdef WIN32
#define _WIN32_WINNT 0x400      /* Require Windows NT5 (2000, XP, 2003) for SignalObjectAndWait */
#include <winsock2.h>
#include <windows.h>
#include "getopt.h"
#else
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/resource.h>
#if !defined(__VMS) && !defined(SOLCLIENT_AIX_BUILD) && !defined(SOLCLIENT_ZTPF_BUILD) && !defined(SOLCLIENT_HPUX_BUILD)
#include <getopt.h>
#include <sys/syscall.h>
#else
#include "getopt.h"
#endif
#include <netinet/in.h>
#endif

#ifdef WIN32
#   define strncpy(dest_p, src_p, size) (strncpy_s(dest_p, size, src_p, _TRUNCATE))
#   define strcasecmp (_stricmp)
#   define strncasecmp (_strnicmp)
#   define snprintf(buf_p, size, ...) (_snprintf_s(buf_p, size, _TRUNCATE, __VA_ARGS__))
#   define vsnprintf(buf_p, size, ...) (_vsnprintf_s(buf_p, size, _TRUNCATE, __VA_ARGS__))
    typedef DWORD   threadRetType;
#	define DEFAULT_THREAD_RETURN_ARG (0)
    typedef DWORD   pthread_t;
#   define write(fd, buf_p, len) (send(fd, buf_p, (int)len, 0))
#   define read(fd, buf_p, len) (recv(fd, buf_p, (int)len, 0))
#   define close (closesocket)
    typedef unsigned int ssize_t;
#   define FOPEN(fd, fname, mode) (fopen_s(&fd, fname, mode))
    typedef LPTHREAD_START_ROUTINE threadFn_pt;
#else
    typedef void   *threadRetType;
#	define DEFAULT_THREAD_RETURN_ARG (NULL)
#if !defined(SOLCLIENT_AIX_BUILD) && !defined(SOLCLIENT_HPUX_BUILD)
#   define FOPEN(fd, fname, mode) fd = fopen(fname, mode)
#endif
    typedef         threadRetType ( *threadFn_pt ) ( void *param_p );
#endif

/* To allow network byte order translation of INT64. */
#if !defined(SOLCLIENT_AIX_BUILD) && !defined(SOLCLIENT_HPUX_BUILD)
#define ntohll(x) (((INT64)(ntohl((int)((x << 32) >> 32))) << 32) | (unsigned int)ntohl(((int)(x >> 32))))
#define htonll(x) ntohll(x)
#endif

/* Data types */
#ifdef WIN32
    typedef unsigned __int64 UINT64;
    typedef HANDLE  MUTEX_T;
#define PTHREAD_MUTEX_INITIALIZER (NULL)
    typedef HANDLE  CONDITION_T;
#define PTHREAD_COND_INITIALIZER (NULL)
    typedef HANDLE  SEM_T;
    typedef HANDLE  THREAD_HANDLE_T;
#elif defined(DARWIN_OS)
	typedef uint64_t UINT64;
	typedef pthread_mutex_t MUTEX_T;
	typedef struct {
		char name[9]; // FFFFFFFF
		sem_t *semaphore;
	} SEM_T;
	typedef pthread_t THREAD_HANDLE_T;
	typedef pthread_cond_t CONDITION_T;
#else
    typedef unsigned long long UINT64;
    typedef pthread_mutex_t MUTEX_T;
    typedef sem_t   SEM_T;
    typedef pthread_t THREAD_HANDLE_T;
    typedef pthread_cond_t CONDITION_T;
#endif


/* Semaphore that will be posted to when CTRL-C is hit. */
    extern SEM_T    ctlCSem;
    extern BOOL     gotCtlC;

/*
 * fn initSigHandler()
 * Intiailize CTRL-C handler.
 */
    void            initSigHandler ( void );

/* argv bit 32-64 abstraction */
    int             getArgVOS(int argc, charPtr32* argv, ptr* argVOS);

/*
 * fn sleepInSec()
 * Suspends the execution of the current thread until the time-out interval elapses.
 * param secToSleep number of seconds to sleep.
 */
    void            sleepInSec ( int secToSleep );

/*
 * fn sleepInUs()
 * Suspends the execution of the current thread until the time-out interval elapses.
 * param usToSleep number of microseconds to sleep.
 */
    void            sleepInUs ( int usToSleep );

/*
 * fn getTimeInUs()
 * Gets the current time (in microseconds).
 */
    UINT64          getTimeInUs ( void );

/*
 * fn _getDateTime()
 * Returns a string representation of current time.
 */
    void            _getDateTime ( char *buf_p, int bufSize );
/*
 * fn waitUntil()
 * Wait until the given time has occured, returning the amount of time waited.
 * param nexttimeInUs time to return at.
 */
    UINT64          waitUntil ( UINT64 nexttimeInUs );

/*
 * fn getUsageTime()
 *
 * param userTime_p
 * param systemTime_p
 */
    void            getUsageTime ( long long *userTime_p, long long *systemTime_p );

/*
 * getCpuSpeedInHz()
 * Gets the cpu speed in Hertz.
 * Warning: Only available for WIN32 and Linux.
 */
    UINT64          getCpuSpeedInHz ( void );

/*
 * fn mutexInit()
 * Initialize a mutex.
 * param mutex_p returned mutex handle.
 */
    BOOL            mutexInit ( MUTEX_T * mutex_p );

/*
 * mutexDestroy()
 * Destroy a mutex.
 * param mutex_p the mutex to destroy.
 */
    BOOL            mutexDestroy ( MUTEX_T * mutex_p );

/*
 * fn mutexLock()
 * Locks the mutex, perhaps waiting indefinitely for the lock.
 * param mutex_p the mutex to lock.
 * Return TRUE on success.
 */
    BOOL            mutexLock ( MUTEX_T * mutex_p );

/*
 * fn mutexUnlock()
 * Unlock a mutex.
 * param mutex_p the mutex to unlock.
 * Return TRUE on success.
 */
    BOOL            mutexUnlock ( MUTEX_T * mutex_p );

/*
 * fn condInit()
 * Initialize a condition variable.
 * param cond_p the condition variable to initialize.
 * Return TRUE on success.
 */
    BOOL            condInit ( CONDITION_T * cond_p );

/*
 * fn condReset()
 * Resets a condition variable to the non-signaled state.
 * param cond_p the condition variable to reset.
 * Return TRUE on success.
 */
    BOOL            condReset ( CONDITION_T * cond_p );

/*
 * fn condWait()
 * Waits for a condition variable to be set. 
 * The mutex is locked on the exit of this function.
 * param cond_p the condition variable waiting to be set.
 * param mutex_p the mutex to be locked.
 * Return TRUE on success.
 */
    BOOL            condWait ( CONDITION_T * cond_p, MUTEX_T * mutex_p );

/*
 * fn condDestroy()
 * Destroy a condition variable.
 * param cond_p the condition variable to destroy.
 * Return TRUE on success.
 */
    BOOL            condDestroy ( CONDITION_T * cond_p );

/*
 * fn condTimedWait()
 * Wait for a condition variable to be signalled (or a timeout).
 * param cond_p condition variable.
 * param mutex_p condition variable's mutex.
 * param timeoutSec timeout in seconds of the wait (negative means forever).
 * Return TRUE on success.
 */
    BOOL            condTimedWait ( CONDITION_T * cond_p, MUTEX_T * mutex_p, int timeoutSec     /* negative means forever */
             );

/*
 * fn condSignal()
 * Signal a condition variable.
 * param cond_p condition variable to signal.
 * Return TRUE on success.
 */
    BOOL            condSignal ( CONDITION_T * cond_p );

/*
 * fn semInit()
 * Creata a semaphore.
 * Return TRUE on success.
 */
                    BOOL semInit ( SEM_T * sem_p, unsigned int initValue, unsigned int maxValue );

/*
 * fn semDestroy()
 * Destroy a semaphore that was previously created.
 * Return TRUE on success.
 */
                    BOOL semDestroy ( SEM_T * sem_p );

/*
 * fn semWait()
 * Wait on a semaphore (wait forever).
 * Return TRUE on success.
 */
                    BOOL semWait ( SEM_T * sem_p );

/*
 * fn semPost()
 * Post to a semaphore (increase semaphore value by 1).
 * Return TRUE on success.
 */
                    BOOL semPost ( SEM_T * sem_p );


/* Thread abstraction */
#define _NULL_THREAD_ID (0)
#ifdef WIN32
    typedef void    FP ( void * );
#else
    typedef void   *( *FP ) ( void * );
#endif

/*
 * fn startThread()
 * Start a new thread.
 * param fp the thread function to be executed.
 * param arg the argument to pass to the thread function.
 * Return the thread handle.
 */
    THREAD_HANDLE_T startThread ( FP fp, void *arg );

/*
 * fn waitOnThread()
 * Waits on the thread.
 * param handle Thread handle to wait on.
 */
    void            waitOnThread ( THREAD_HANDLE_T handle );

#ifdef __cplusplus
}
#endif

#endif
