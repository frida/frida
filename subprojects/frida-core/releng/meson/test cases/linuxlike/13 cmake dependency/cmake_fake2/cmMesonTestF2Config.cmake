find_package(ZLIB)

if(ZLIB_FOUND OR ZLIB_Found)
  set(cmMesonTestF2_FOUND        ON)
  set(cmMesonTestF2_LIBRARIES    ${ZLIB_LIBRARY})
  set(cmMesonTestF2_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
else()
  set(cmMesonTestF2_FOUND       OFF)
endif()
