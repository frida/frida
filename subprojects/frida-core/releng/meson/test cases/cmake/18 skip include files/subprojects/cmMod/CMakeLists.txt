cmake_minimum_required(VERSION 3.5)

project(cmMod)
set (CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions("-DDO_NOTHING_JUST_A_FLAG=1")

set(SRCS
  ${CMAKE_CURRENT_LIST_DIR}/cmMod.hpp
  ${CMAKE_CURRENT_LIST_DIR}/cmMod.cpp
)

add_subdirectory(fakeInc)
