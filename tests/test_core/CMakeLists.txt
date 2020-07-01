file(GLOB SRCS *.cpp)

include_directories(../../src/core)

add_executable(TestCore ${SRCS})


target_link_libraries(TestCore gtest gtest_main)
target_link_libraries(TestCore ${CMAKE_PROJECT_NAME})

add_test(NAME TestCore
         COMMAND TestCore)