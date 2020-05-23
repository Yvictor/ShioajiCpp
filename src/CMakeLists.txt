project(${CMAKE_PROJECT_NAME}_lib)

set(HEADER_FILES core/sol.h)
set(SOURCE_FILES core/sol.cpp)
add_library(ShioajiCpp SHARED ${SOURCE_FILES} ${HEADERS_FILES})