find_package(ZLIB)

if(ZLIB_FOUND OR ZLIB_Found)
  set(cmMesonVersionedTestDep_FOUND        ON)
  set(cmMesonVersionedTestDep_LIBRARIES    ${ZLIB_LIBRARY})
  set(cmMesonVersionedTestDep_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
else()
  set(cmMesonVersionedTestDep_FOUND       OFF)
endif()
