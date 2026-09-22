cmake_minimum_required(VERSION 3.7)
project(cmObject CXX)

find_package(ZLIB REQUIRED)

add_library(lib_obj OBJECT libA.cpp libB.cpp)
add_library(lib_sha SHARED $<TARGET_OBJECTS:lib_obj>)
add_library(lib_sta STATIC $<TARGET_OBJECTS:lib_obj>)

target_link_libraries(lib_sha ZLIB::ZLIB)
target_link_libraries(lib_sta ZLIB::ZLIB)
