cmake_minimum_required(VERSION 3.5)

project(cmMod)
set (CMAKE_CXX_STANDARD 14)

add_definitions("-DDO_NOTHING_JUST_A_FLAG=1")

add_library(cmModLib INTERFACE)
set_target_properties(cmModLib PROPERTIES INTERFACE_COMPILE_OPTIONS "-DCMAKE_FLAG_MUST_BE_PRESENT")
target_include_directories(cmModLib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(cmModLib INTERFACE -DCMAKE_COMPILER_DEFINE_STR="compDef")
target_compile_definitions(cmModLib INTERFACE MESON_MAGIC_FLAG=42)
