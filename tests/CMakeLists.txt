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
include_directories(../include)


add_executable(TestShioaji test_shioaji.cpp)
target_link_libraries(TestShioaji gtest gtest_main)
target_link_libraries(TestShioaji ${CMAKE_PROJECT_NAME})

add_test(NAME TestShioaji
        COMMAND TestShioaji)

add_subdirectory(test_core)