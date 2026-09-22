cmake_minimum_required(VERSION 3.5)

project(cmMod)
set (CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions("-DDO_NOTHING_JUST_A_FLAG=1")

add_library(cmModLib SHARED cmMod.cpp)
include(GenerateExportHeader)
generate_export_header(cmModLib)

target_compile_options(cmModLib PRIVATE "-Wall" "-Werror")
target_include_directories(cmModLib SYSTEM PRIVATE "sysInc")
