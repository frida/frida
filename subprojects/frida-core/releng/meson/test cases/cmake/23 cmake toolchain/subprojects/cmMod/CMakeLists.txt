cmake_minimum_required(VERSION 3.5)

project(cmMod NONE)

if(NOT "${MESON_TEST_VAR1}" STREQUAL "VAR1 space")
  message(FATAL_ERROR "MESON_TEST_VAR1 -- '${MESON_TEST_VAR1}' != 'VAR1 space'")
endif()

if(NOT "${MESON_TEST_VAR2}" STREQUAL "VAR2")
  message(FATAL_ERROR "MESON_TEST_VAR2 -- '${MESON_TEST_VAR2}' != 'VAR2'")
endif()

if(NOT DEFINED CMAKE_C_COMPILER_VERSION)
  message(FATAL_ERROR "CMAKE_C_COMPILER_VERSION was not defined")
endif()
