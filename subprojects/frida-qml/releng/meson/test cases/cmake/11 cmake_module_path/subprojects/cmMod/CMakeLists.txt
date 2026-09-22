cmake_minimum_required(VERSION 3.5)

project(cmMod)

message(STATUS "CMAKE_MODULE_PATH: '${CMAKE_MODULE_PATH}'")

find_package(SomethingLikePython REQUIRED)

add_custom_command(
  OUTPUT            "${CMAKE_CURRENT_BINARY_DIR}/main.c"
  COMMAND           Python::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/gen.py"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(main "${CMAKE_CURRENT_BINARY_DIR}/main.c")
