cmake_minimum_required(VERSION 3.5)

project(cmTest)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(cmTest STATIC cmTest.c cmTest.m)
target_compile_definitions(cmTest PUBLIC SOME_MAGIC_DEFINE=42)
