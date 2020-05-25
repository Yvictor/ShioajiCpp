project(${CMAKE_PROJECT_NAME}_lib)


set(SOLCLIENT_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/libs/solace/solclient_Linux26-x86_64_opt_7.13.0.4/solclient-7.13.0.4/include)
set(SOLCLIENT_LIB_PATH ${CMAKE_SOURCE_DIR}/libs/solace/solclient_Linux26-x86_64_opt_7.13.0.4/solclient-7.13.0.4/lib)
set(SOLCLIENT_LIB_NAME libsolclient.a)
include_directories(${SOLCLIENT_INCLUDE_PATH})
add_library(solclient STATIC IMPORTED)
set_property(TARGET solclient PROPERTY IMPORTED_LOCATION ${SOLCLIENT_LIB_PATH}/${SOLCLIENT_LIB_NAME})

set(HEADER_FILES core/sol.h core/libsolclient.h )
set(SOURCE_FILES core/sol.cpp core/solmsg.cpp core/solmsg.h)
add_library(ShioajiCpp STATIC ${SOURCE_FILES} ${HEADERS_FILES})
target_link_libraries(ShioajiCpp solclient rt)
