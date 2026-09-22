find_package(ZLIB)

include(CMakeFindDependencyMacro)
include(CheckCXXSourceRuns)
include(CheckCSourceRuns)

# Do something stupid (see https://github.com/mesonbuild/meson/issues/7501)
set("")

check_cxx_source_runs(
"
#include <iostream>

using namespace std;

int main(void) {
  cout << \"Hello World\" << endl;
  return 0;
}
"
CXX_CODE_RAN
)

check_c_source_runs(
"
#include <stdio.h>

int main(void) {
  printf(\"Hello World\");
  return 0;
}
"
C_CODE_RAN
)

if(NOT CXX_CODE_RAN)
  message(FATAL_ERROR "Running CXX source code failed")
endif()

if(NOT C_CODE_RAN)
  message(FATAL_ERROR "Running C source code failed")
endif()

if(NOT SomethingLikeZLIB_FIND_COMPONENTS STREQUAL "required_comp")
  message(FATAL_ERROR "Component 'required_comp' was not specified")
endif()

find_dependency(Threads)

if(ZLIB_FOUND OR ZLIB_Found)
  set(SomethingLikeZLIB_FOUND        ON)
  set(SomethingLikeZLIB_LIBRARIES    ${ZLIB_LIBRARY})
  set(SomethingLikeZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
else()
  set(SomethingLikeZLIB_FOUND       OFF)
endif()
