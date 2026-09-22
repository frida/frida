cmake_minimum_required(VERSION 3.5)

project(cmMod C CXX)
set (CMAKE_CXX_STANDARD 14)

if(NOT USE_PTHREAD STREQUAL NOT_SET)
  set(THREADS_PREFER_PTHREAD_FLAG ${USE_PTHREAD})
endif()
find_package(Threads)

add_library(cmModLib STATIC cmMod.cpp)
target_link_libraries(cmModLib PRIVATE Threads::Threads)

add_library(cmModLib_shared SHARED cmMod.cpp)
target_link_libraries(cmModLib_shared PUBLIC Threads::Threads)
