cmake_minimum_required(VERSION 3.16)
project(ShioajiCpp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
elseif (UNIX AND APPLE)
    set(DARWIN TRUE)
endif()

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

set(HEADER_FILES src/core/libsolclient.h src/core/sol.h src/core/solmsg.h src/shioaji_impl.h include/shioaji.h)
set(SOURCE_FILES src/core/sol.cpp src/core/solmsg.cpp src/shioaji_impl.cpp)
add_library(ShioajiCpp SHARED ${SOURCE_FILES} ${HEADERS_FILES})
target_link_libraries(ShioajiCpp solclient ${LINK_LIBS})

enable_testing()
add_subdirectory(tests)
if(MSVC)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug)
    endif()
    add_custom_command(
        TARGET ShioajiCpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/ShioajiCpp.dll
        ${CMAKE_CURRENT_BINARY_DIR}/tests
    )
    add_custom_command(
            TARGET ShioajiCpp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/ShioajiCpp.dll
            ${CMAKE_CURRENT_BINARY_DIR}/tests/test_core
    )
#    install(TARGETS ShioajiCpp RUNTIME DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests/${CMAKE_BUILD_TYPE}")
#    install(TARGETS ShioajiCpp RUNTIME DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/tests/test_core/${CMAKE_BUILD_TYPE}")
#    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/libShioajiCpp.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()

