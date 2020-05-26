project(${CMAKE_PROJECT_NAME}_lib)

set(SOLCLIENT_VERSION 7.13.0.4)
set(SOLCLIENT_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/libs/solace/include/solclient-${SOLCLIENT_VERSION})
include_directories(${SOLCLIENT_INCLUDE_PATH})

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
elseif (UNIX AND APPLE)
    set(DARWIN TRUE)
endif()

if (DARWIN)
    set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/Darwin/solclient-${SOLCLIENT_VERSION}/lib)
    set(SOLCLIENT_LIB_NAME libsolclient.a)
elseif(LINUX)
    set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/Linux/solclient-${SOLCLIENT_VERSION}/lib)
    set(SOLCLIENT_LIB_NAME libsolclient.a)
    set(LINK_LIBS rt)
elseif(MSYS)
endif()

add_library(solclient STATIC IMPORTED)
set_property(TARGET solclient PROPERTY IMPORTED_LOCATION ${SOLCLIENT_LIB_PATH}/${SOLCLIENT_LIB_NAME})

set(HEADER_FILES core/sol.h core/libsolclient.h )
set(SOURCE_FILES core/sol.cpp core/solmsg.cpp core/solmsg.h)
add_library(ShioajiCpp STATIC ${SOURCE_FILES} ${HEADERS_FILES})
target_link_libraries(ShioajiCpp solclient ${LINK_LIBS})
