cmake_minimum_required(VERSION 3.16)
project(ShioajiCpp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
elseif (UNIX AND APPLE)
    set(DARWIN TRUE)
endif()

if (DARWIN)
#execute_process(COMMAND xcodebuild -version -sdk macosx Path OUTPUT_VARIABLE SDK_ROOT)
#message("SDKROOT=${SDK_ROOT}")
#set(ENV{SDKROOT} ${SDK_ROOT})
endif()


#if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
#    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
#    set(CONAN_VERSION 0.15)
#    if (DARWIN)
#        set(CONAN_VERSION 0.13) ## See: https://github.com/conan-io/cmake-conan/issues/159
#    endif()
#    file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v${CONAN_VERSION}/conan.cmake"
#            "${CMAKE_BINARY_DIR}/conan.cmake")
#endif()
#set(CONAN_CMD conan)
#include(${CMAKE_BINARY_DIR}/conan.cmake)


set(SOLCLIENT_VERSION 7.13.0.4)
if (DARWIN)
    set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/Darwin/solclient-${SOLCLIENT_VERSION}/lib)
    set(SOLCLIENT_LIB_NAME libsolclient.so.1)
    # temp resloving so not foud issue on mac os
    file(COPY ${SOLCLIENT_LIB_PATH}/${SOLCLIENT_LIB_NAME} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)
    file(COPY ${SOLCLIENT_LIB_PATH}/${SOLCLIENT_LIB_NAME} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/test_core)
elseif(LINUX)
    set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/Linux/solclient-${SOLCLIENT_VERSION}/lib)
    set(SOLCLIENT_LIB_NAME libsolclient.so)
    set(LINK_LIBS rt)
elseif(MSVC)
    set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/Windows/solclient-${SOLCLIENT_VERSION})
    set(SOLCLIENT_LIB_NAME libsolclient.lib)
    file(COPY ${SOLCLIENT_LIB_PATH}/bin/Win64/libsolclient.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)
    file(COPY ${SOLCLIENT_LIB_PATH}/bin/Win64/libsolclient.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/test_core)
endif()
message("${CMAKE_SOURCE_DIR}")
message("${SOLCLIENT_LIB_PATH}")

add_library(solclient SHARED IMPORTED)

if (MSVC)
    set_property(TARGET solclient PROPERTY IMPORTED_LOCATION ${SOLCLIENT_LIB_PATH}/bin/Win64)
    set_property(TARGET solclient PROPERTY IMPORTED_IMPLIB ${SOLCLIENT_LIB_PATH}/lib/Win64/${SOLCLIENT_LIB_NAME})
    link_directories(${SOLCLIENT_LIB_PATH}/lib/Win64)
else()
    set_property(TARGET solclient PROPERTY IMPORTED_LOCATION ${SOLCLIENT_LIB_PATH}/${SOLCLIENT_LIB_NAME})
    link_directories(${SOLCLIENT_LIB_PATH})
endif()

set(SOLCLIENT_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/libs/solace/include/solclient-${SOLCLIENT_VERSION})
include_directories(${SOLCLIENT_INCLUDE_PATH})

include_directories(src)
include_directories(include)

#conan_cmake_run(REQUIRES spdlog/1.6.1
#                BASIC_SETUP
#                BUILD missing
#)
# Find packages.
find_package(spdlog REQUIRED)
#find_package(spdlog CONFIG REQUIRED)
find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h)
find_library(SPDLOG_LIBRARY spdlog)
find_package(spdlog CONFIG REQUIRED)

set(HEADER_FILES src/core/libsolclient.h src/core/sol.h src/core/solmsg.h src/shioaji_impl.h src/core/session.h include/shioaji.h)
set(SOURCE_FILES src/core/sol.cpp src/core/solmsg.cpp src/shioaji_impl.cpp src/core/session.cpp)
add_library(ShioajiCpp SHARED ${SOURCE_FILES} ${HEADERS_FILES})
message("CONFIG=${CONFIG}")
set_target_properties(ShioajiCpp
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin
        )
target_include_directories(ShioajiCpp PUBLIC ${SPDLOG_INCLUDE_DIR})
target_link_libraries(ShioajiCpp PRIVATE solclient spdlog::spdlog spdlog::spdlog_header_only)

enable_testing()
add_subdirectory(tests)
message(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>")
message("LIBRARY_OUTPUT_DIRECTORY:${LIBRARY_OUTPUT_DIRECTORY}")
message("LIBRARY_OUTPUT_NAME:${LIBRARY_OUTPUT_NAME}")
message("RUNTIME_OUTPUT_DIRECTORY:${RUNTIME_OUTPUT_DIRECTORY}")
message("RUNTIME_OUTPUT_NAME:${RUNTIME_OUTPUT_NAME}")
message("CMAKE_CURRENT_BINARY_DIR:${CMAKE_CURRENT_BINARY_DIR}")
message("=====================================================")

if(MSVC)
    #    if(NOT CMAKE_BUILD_TYPE)
#        set(CMAKE_BUILD_TYPE Debug)
#    endif()
#    add_custom_command(
#        TARGET ShioajiCpp POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy
#        ${RUNTIME_OUTPUT_DIRECTORY}/ShioajiCpp.dll
#        ${CMAKE_CURRENT_BINARY_DIR}/tests
#    )

    add_custom_command(
            TARGET ShioajiCpp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:ShioajiCpp>
            ${CMAKE_CURRENT_BINARY_DIR}/tests
    )

    add_custom_command(
            TARGET ShioajiCpp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:ShioajiCpp>
            ${CMAKE_CURRENT_BINARY_DIR}/tests/test_core
    )
#    install(TARGETS ShioajiCpp RUNTIME DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests/${CMAKE_BUILD_TYPE}")
#    install(TARGETS ShioajiCpp RUNTIME DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests/test_core/${CMAKE_BUILD_TYPE}")

#    message("RUNTIME_OUTPUT_DIRECTORY:${RUNTIME_OUTPUT_DIRECTORY}")
#    message("RUNTIME_OUTPUT_NAME:${RUNTIME_OUTPUT_NAME}")
#    message("CMAKE_CURRENT_BINARY_DIR:${CMAKE_CURRENT_BINARY_DIR}")
#    EXEC_PROGRAM(dir ARGS ${CMAKE_CURRENT_BINARY_DIR} OUTPUT_VARIABLE LS_OUTPUT RETURN_VALUE LS_RVALUE)
#    MESSAGE(STATUS "dir result: " ${LS_RVALUE}) # 縮排僅為美觀,語法無要求
#    set(rt "NOFOUND")
#    find_file(rt "ShioajiCpp.dll")
#    file(COPY ${rt} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()

