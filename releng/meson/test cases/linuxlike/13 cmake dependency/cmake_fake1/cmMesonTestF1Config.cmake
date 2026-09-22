find_package(ZLIB)

if(ZLIB_FOUND OR ZLIB_Found)
  set(cmMesonTestF1_FOUND        ON)
  set(cmMesonTestF1_LIBRARIES    general ${ZLIB_LIBRARY})
  set(cmMesonTestF1_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})

  add_library(CMMesonTESTf1::evil_non_standard_target UNKNOWN IMPORTED)
else()
  set(cmMesonTestF1_FOUND       OFF)
endif()
