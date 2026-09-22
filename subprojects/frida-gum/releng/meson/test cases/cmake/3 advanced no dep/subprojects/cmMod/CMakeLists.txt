cmake_minimum_required(VERSION 3.5)

project(cmMod)
set(CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(CONFIG_OPT 42)
configure_file("config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY)

add_library(cmModLib       SHARED lib/cmMod.cpp)
add_library(cmModLibStatic STATIC lib/cmMod.cpp)
include(GenerateExportHeader)
generate_export_header(cmModLib)

set_target_properties(cmModLib PROPERTIES VERSION 1.0.1)

add_executable(meson-testEXE main.cpp)
add_executable(benchmark main.cpp)

target_link_libraries(meson-testEXE cmModLib)
target_link_libraries(benchmark cmModLib)

target_compile_definitions(cmModLibStatic PUBLIC CMMODLIB_STATIC_DEFINE)

install(TARGETS meson-testEXE benchmark LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
