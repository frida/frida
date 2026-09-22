list(APPEND SRCS
  cmModInc1.cpp
  cmModInc2.cpp
  cmModInc3.cpp
  cmModInc4.cpp
)

set(SRC_A
  cmModInc1.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cmModInc2.cpp
)

set_property(
  SOURCE ${SRC_A}
  PROPERTY
    HEADER_FILE_ONLY ON
)

set_source_files_properties(
  cmModInc3.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cmModInc4.cpp
  PROPERTIES
    LABELS "CMake;Lists;are;fun"
    HEADER_FILE_ONLY ON
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(cmModLib++ SHARED ${SRCS})
include(GenerateExportHeader)
generate_export_header(cmModLib++)
