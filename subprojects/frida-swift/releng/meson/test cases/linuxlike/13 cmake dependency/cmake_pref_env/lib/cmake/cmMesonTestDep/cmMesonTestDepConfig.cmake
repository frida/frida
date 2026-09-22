find_package(ZLIB)

if(ZLIB_FOUND OR ZLIB_Found)
  set(cmMesonTestDep_FOUND        ON)
  set(cmMesonTestDep_LIBRARIES    ${ZLIB_LIBRARY})
  set(cmMesonTestDep_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
else()
  set(cmMesonTestDep_FOUND       OFF)
endif()
