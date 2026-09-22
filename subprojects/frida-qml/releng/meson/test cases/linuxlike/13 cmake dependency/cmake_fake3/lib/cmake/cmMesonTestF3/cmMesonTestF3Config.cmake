find_package(ZLIB)

if(ZLIB_FOUND OR ZLIB_Found)
  set(cmMesonTestF3_FOUND        ON)
  set(cmMesonTestF3_LIBRARIES    ${ZLIB_LIBRARY})
  set(cmMesonTestF3_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
else()
  set(cmMesonTestF3_FOUND       OFF)
endif()
