cmake_minimum_required(VERSION 3.5)

project(cmMod)
set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_definitions("-DDO_NOTHING_JUST_A_FLAG=1")

add_executable(genMain genMain.cpp)
add_custom_command(OUTPUT main.cpp COMMAND genMain > main.cpp)

add_executable(gen main.cpp)
add_executable(mycpy cp.cpp)

# cpyBase
add_custom_command(
  OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/genTest.cpp" "${CMAKE_CURRENT_BINARY_DIR}/genTest.hpp"
  COMMAND gen ARGS genTest
)

set(CMD_PART)
list(APPEND CMD_PART COMMAND mycpy cpyBase.cpp.in     cpyBase.cpp.in.gen)
list(APPEND CMD_PART COMMAND mycpy cpyBase.cpp.in.gen cpyBase.cpp.out)
list(APPEND CMD_PART COMMAND mycpy cpyBase.cpp.out    cpyBase.cpp.something)

add_custom_command(
  OUTPUT cpyBase.cpp
  COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyBase.cpp.am" cpyBase.cpp.in
  ${CMD_PART}
  COMMAND mycpy cpyBase.cpp.in                               cpyBase.cpp.something
  COMMAND mycpy cpyBase.cpp.something                        cpyBase.cpp.IAmRunningOutOfIdeas
  COMMAND mycpy cpyBase.cpp.IAmRunningOutOfIdeas             cpyBase.cpp
  DEPENDS cpyBase.cpp.am;gen
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyBase.hpp.in"
  COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyBase.hpp.am" cpyBase.hpp.in
  DEPENDS cpyBase.hpp.am
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyBase.hpp.something"
  COMMAND mycpy cpyBase.hpp.in                               cpyBase.hpp.something
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cpyBase.hpp.in"
)

# Slight tuning to the file extension (it's still a C++ header)
# checks that file extensions are not considered for the purposes
# of target dependencies
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyBase.txt"
  COMMAND mycpy cpyBase.hpp.something                        cpyBase.txt
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cpyBase.hpp.something"
)

# cpyNext (out of order is on purpose)
# -- first copy round
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/s1_a_hpp/file.txt"
  COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyNext.hpp.am" file.txt
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cpyNext.hpp.am"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/s1_a_hpp"
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/s1_b_cpp/file.txt"
  COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyNext.cpp.am" file.txt
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cpyNext.cpp.am"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/s1_b_cpp"
)

# -- final cpy round
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyNext.hpp"
  COMMAND mycpy "${CMAKE_CURRENT_BINARY_DIR}/s2_b_hpp/file.txt" cpyNext.hpp
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/s2_b_hpp/file.txt"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyNext.cpp"
  COMMAND mycpy "${CMAKE_CURRENT_BINARY_DIR}/s2_a_cpp/file.txt" cpyNext.cpp
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/s2_a_cpp/file.txt"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# -- second copy round
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/s2_b_hpp/file.txt"
  COMMAND mycpy "${CMAKE_CURRENT_BINARY_DIR}/s1_a_hpp/file.txt" file.txt
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/s1_a_hpp/file.txt"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/s2_b_hpp"
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/s2_a_cpp/file.txt"
  COMMAND mycpy "${CMAKE_CURRENT_BINARY_DIR}/s1_b_cpp/file.txt" file.txt
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/s1_b_cpp/file.txt"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/s2_a_cpp"
)

# cpyTest (copy file without renaming)
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyTest.hpp"
  COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest/cpyTest.hpp" "${CMAKE_CURRENT_BINARY_DIR}/cpyTest.hpp"
  DEPENDS "cpyTest/cpyTest.hpp"
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyTest2.hpp"
  COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest/cpyTest2.hpp" "${CMAKE_CURRENT_BINARY_DIR}/cpyTest2.hpp"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest/cpyTest2.hpp"
)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyTest3.hpp"
  COMMAND mycpy cpyTest3.hpp "${CMAKE_CURRENT_BINARY_DIR}/cpyTest3.hpp"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest/cpyTest3.hpp"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest"
)

add_subdirectory(cpyTest ccppyyTTeesstt)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyTest/some/directory/cpyTest5.hpp"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest/cpyTest5.hpp" "${CMAKE_CURRENT_BINARY_DIR}/cpyTest/some/directory/cpyTest5.hpp"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest/cpyTest5.hpp"
)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/cpyTest/some")

add_library(cmModLib SHARED cmMod.cpp genTest.cpp cpyBase.cpp cpyBase.txt cpyNext.cpp cpyNext.hpp cpyTest.cpp cpyTest.hpp cpyTest2.hpp cpyTest3.hpp cpyTest/some/directory/cpyTest5.hpp)
include(GenerateExportHeader)
generate_export_header(cmModLib)

set(ARGS_TEST arg1)
set(ARGS_TEST ${ARGS_TEST} arg2)

add_executable(macro_name macro_name.cpp)
add_executable(args_test args_test.cpp)
add_custom_target(args_test_cmd
  COMMAND args_test ${ARGS_TEST}
)
add_custom_target(macro_name_cmd COMMAND macro_name)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  message(STATUS "Running the -include test case on macro_name")
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cpyInc.hpp"
    COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyInc.hpp.am" "${CMAKE_CURRENT_BINARY_DIR}/cpyInc.hpp"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cpyInc.hpp.am"
  )
  target_compile_options(macro_name PUBLIC -DTEST_CMD_INCLUDE -include "${CMAKE_CURRENT_BINARY_DIR}/cpyInc.hpp")
endif()

# Only executable targets are replaced in the command
# all other target names are kept as is
add_custom_target(clang-format COMMAND clang-format -i cmMod.cpp)

add_dependencies(cmModLib args_test_cmd tgtCpyTest4)
add_dependencies(args_test_cmd macro_name_cmd;gen;mycpy)

# Reproduce https://github.com/mesonbuild/meson/issues/10244
add_custom_target(mycpy.all)
add_dependencies(mycpy.all mycpy)
