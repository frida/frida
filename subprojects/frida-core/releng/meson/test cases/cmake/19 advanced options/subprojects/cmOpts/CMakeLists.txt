cmake_minimum_required(VERSION 3.7)

project(CmOpts)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT "${SOME_CMAKE_VAR}" STREQUAL "something")
  message(FATAL_ERROR "Setting the CMake var failed")
endif()

add_library(cmModLib++ STATIC cmMod.cpp)
add_library(cmTestLib  STATIC cmTest.cpp)
add_executable(testEXE main.cpp)

target_link_libraries(testEXE cmModLib++)

install(TARGETS cmTestLib ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
