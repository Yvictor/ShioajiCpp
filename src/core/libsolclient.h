//
// Created by 011295 on 2020/5/25.
//

#ifndef SHIOAJICPP_SRC_CORE_LIBSOLCLIENT_H_
#define SHIOAJICPP_SRC_CORE_LIBSOLCLIENT_H_
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#endif
#if defined(_WIN32) || defined(_WIN64)
// must include first
#include <winsock2.h>
// must include second
#include <windows.h>
// prevent fromat sorting include order
#pragma warning(disable : 4819)
#pragma warning(disable : 4996)
#include <process.h>
#include <synchapi.h>
#else
#include <unistd.h>
#endif

#include <solclient/solClient.h>
#include <solclient/solClientMsg.h>
#ifdef __cplusplus
#define CC "C"
#else
#define CC
#endif

#endif //SHIOAJICPP_SRC_CORE_LIBSOLCLIENT_H_
