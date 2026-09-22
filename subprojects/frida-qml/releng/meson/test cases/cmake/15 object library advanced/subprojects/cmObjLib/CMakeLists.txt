cmake_minimum_required(VERSION 3.7)
project(cmObject CXX)

add_executable(genC genC.cpp)

add_custom_command(
  OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/libC.cpp" "${CMAKE_CURRENT_BINARY_DIR}/libC.hpp"
  COMMAND genC
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(lib_obj OBJECT libA.cpp libB.cpp "${CMAKE_CURRENT_BINARY_DIR}/libC.cpp" "${CMAKE_CURRENT_BINARY_DIR}/libC.hpp")
add_library(lib_sha SHARED $<TARGET_OBJECTS:lib_obj>)
add_library(lib_sta STATIC $<TARGET_OBJECTS:lib_obj>)

target_compile_definitions(lib_obj PRIVATE "-DBUILD_AS_OBJ=1")
