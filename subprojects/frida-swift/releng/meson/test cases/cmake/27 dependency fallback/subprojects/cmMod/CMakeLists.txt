cmake_minimum_required(VERSION 3.5)

project(cmMod VERSION 1.2.3)
set(CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions("-DDO_NOTHING_JUST_A_FLAG=1")

add_library(cmModLib++ SHARED cmMod.cpp)
target_compile_definitions(cmModLib++ PRIVATE MESON_MAGIC_FLAG=21)
target_compile_definitions(cmModLib++ INTERFACE MESON_MAGIC_FLAG=42)

# Test PCH support
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16.0")
    target_precompile_headers(cmModLib++ PRIVATE "cpp_pch.hpp")
endif()

include(GenerateExportHeader)
generate_export_header(cmModLib++)
