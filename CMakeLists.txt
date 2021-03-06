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
#set(ENV{SDKROOT} $(xcodebuild -version -sdk macosx Path))
endif()


if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    set(CONAN_VERSION 0.15)
    if (DARWIN)
        set(CONAN_VERSION 0.13)
    endif()
    file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v${CONAN_VERSION}/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan.cmake")
endif()
set(CONAN_CMD conan)
include(${CMAKE_BINARY_DIR}/conan.cmake)


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

conan_cmake_run(REQUIRES
            spdlog/1.6.1
            nlohmann_json/3.8.0
            BASIC_SETUP BUILD missing
)
file(GLOB PRIVATE_HEADERS src/*.h src/core/*.h)
message(${PRIVATE_HEADERS})

set(PUBLIC_HEADER include/shioaji.h)
set(SOURCE_FILES src/core/sol.cpp src/core/solmsg.cpp src/shioaji_impl.cpp src/core/session.cpp)
add_library(ShioajiCpp SHARED ${SOURCE_FILES} ${PUBLIC_HEADER} ${PRIVATE_HEADERS})
target_link_libraries(ShioajiCpp solclient ${LINK_LIBS} ${CONAN_LIBS})

enable_testing()
add_subdirectory(tests)
if(MSVC)
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
elseif(DARWIN)
    add_custom_command(
            TARGET ShioajiCpp POST_BUILD
            COMMAND ${CMAKE_INSTALL_NAME_TOOL} -id
            "@loader_path/../lib/$<TARGET_FILE_NAME:ShioajiCpp>"
            $<TARGET_FILE:ShioajiCpp>
    )
endif()

