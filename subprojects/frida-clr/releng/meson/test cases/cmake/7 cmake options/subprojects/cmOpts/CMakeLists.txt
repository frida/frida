cmake_minimum_required(VERSION 3.7)
project(testPro)

if(NOT "${SOME_CMAKE_VAR}" STREQUAL "something")
  message(FATAL_ERROR "Setting the CMake var failed")
endif()

if(NOT "${CMAKE_PREFIX_PATH}" STREQUAL "val1;val2")
  message(FATAL_ERROR "Setting the CMAKE_PREFIX_PATH failed '${CMAKE_PREFIX_PATH}'")
endif()
