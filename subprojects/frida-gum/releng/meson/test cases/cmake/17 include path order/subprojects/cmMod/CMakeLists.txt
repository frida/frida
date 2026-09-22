cmake_minimum_required(VERSION 3.5)

project(cmMod)
set (CMAKE_CXX_STANDARD 14)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}

  # The one and only correct include dir
  ${CMAKE_CURRENT_SOURCE_DIR}/incG

  # All of these are traps
  ${CMAKE_CURRENT_SOURCE_DIR}/incL
  ${CMAKE_CURRENT_SOURCE_DIR}/incM
  ${CMAKE_CURRENT_SOURCE_DIR}/incO
  ${CMAKE_CURRENT_SOURCE_DIR}/incF
  ${CMAKE_CURRENT_SOURCE_DIR}/incI
  ${CMAKE_CURRENT_SOURCE_DIR}/incE
  ${CMAKE_CURRENT_SOURCE_DIR}/incD
  ${CMAKE_CURRENT_SOURCE_DIR}/incH
  ${CMAKE_CURRENT_SOURCE_DIR}/incN
  ${CMAKE_CURRENT_SOURCE_DIR}/incA
  ${CMAKE_CURRENT_SOURCE_DIR}/incB
  ${CMAKE_CURRENT_SOURCE_DIR}/incJ
  ${CMAKE_CURRENT_SOURCE_DIR}/incP
  ${CMAKE_CURRENT_SOURCE_DIR}/incC
  ${CMAKE_CURRENT_SOURCE_DIR}/incK
)

add_definitions("-DDO_NOTHING_JUST_A_FLAG=1")

add_library(cmModLib++ SHARED cmMod.cpp)
include(GenerateExportHeader)
generate_export_header(cmModLib++)
