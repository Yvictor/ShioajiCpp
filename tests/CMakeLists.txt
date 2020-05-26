project(${CMAKE_PROJECT_NAME}_tests)

#set(GOOGLETEST_DIR, ${CMAKE_SOURCE_DIR}/tests/lib/googletest)
#if(NOT EXISTS ${GOOGLETEST_DIR})
#    find_package(Git REQUIRED)
#    execute_process(
#            COMMAND ${GIT_EXECUTABLE} clone https://github.com/google/googletest ${GOOGLETEST_DIR}
#    )
#endif()

add_subdirectory(lib/googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
include_directories(../src/core)

set(SOLCLIENT_VERSION 7.13.0.4)
set(SOLCLIENT_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/libs/solace/include/solclient-${SOLCLIENT_VERSION})
include_directories(${SOLCLIENT_INCLUDE_PATH})

add_executable(Tests_run test_sol.cpp test_solmsg.cpp)
target_link_libraries(Tests_run gtest gtest_main)
target_link_libraries(Tests_run ${CMAKE_PROJECT_NAME})

