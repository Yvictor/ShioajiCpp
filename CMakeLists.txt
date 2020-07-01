cmake_minimum_required(VERSION 3.16)
project(ShioajiCpp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
elseif (UNIX AND APPLE)
    set(DARWIN TRUE)
endif()

set(SOLCLIENT_VERSION 7.13.0.4)
if (DARWIN)
    set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/Darwin/solclient-${SOLCLIENT_VERSION}/lib)
    set(SOLCLIENT_LIB_NAME libsolclient.so.1)
elseif(LINUX)
    set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/Linux/solclient-${SOLCLIENT_VERSION}/lib)
    set(SOLCLIENT_LIB_NAME libsolclient.so)
    set(LINK_LIBS rt)
elseif(MSYS)
endif()
message("${CMAKE_SOURCE_DIR}")
message("${SOLCLIENT_LIB_PATH}")

add_library(solclient SHARED IMPORTED)
set_property(TARGET solclient PROPERTY IMPORTED_LOCATION ${SOLCLIENT_LIB_PATH}/${SOLCLIENT_LIB_NAME})
link_directories(${SOLCLIENT_LIB_PATH}})

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

