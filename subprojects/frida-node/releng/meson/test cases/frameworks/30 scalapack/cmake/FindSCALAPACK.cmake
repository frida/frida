# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:

FindSCALAPACK
-------------

* Michael Hirsch, Ph.D. www.scivision.dev

Let Michael know if there are more MKL / Lapack / compiler combination you want.
Refer to https://software.intel.com/en-us/articles/intel-mkl-link-line-advisor

Finds SCALAPACK libraries for MKL, OpenMPI and MPICH.
Intel MKL relies on having environment variable MKLROOT set, typically by sourcing
mklvars.sh beforehand.

Parameters
^^^^^^^^^^

``MKL``
  Intel MKL for MSVC, ICL, ICC, GCC and PGCC. Working with IntelMPI (default Window, Linux), MPICH (default Mac) or OpenMPI (Linux only).

``IntelMPI``
  MKL BLACS IntelMPI

``MKL64``
  MKL only: 64-bit integers  (default is 32-bit integers)

``OpenMPI``
  SCALAPACK + OpenMPI

``MPICH``
  SCALAPACK + MPICH


Result Variables
^^^^^^^^^^^^^^^^

``SCALAPACK_FOUND``
  SCALapack libraries were found
``SCALAPACK_<component>_FOUND``
  SCALAPACK <component> specified was found
``SCALAPACK_LIBRARIES``
  SCALapack library files
``SCALAPACK_INCLUDE_DIRS``
  SCALapack include directories


References
^^^^^^^^^^

* Pkg-Config and MKL:  https://software.intel.com/en-us/articles/intel-math-kernel-library-intel-mkl-and-pkg-config-tool
* MKL for Windows: https://software.intel.com/en-us/mkl-windows-developer-guide-static-libraries-in-the-lib-intel64-win-directory
* MKL Windows directories: https://software.intel.com/en-us/mkl-windows-developer-guide-high-level-directory-structure
#]=======================================================================]

#===== functions
function(mkl_scala)

set(_mkl_libs ${ARGV})

foreach(s ${_mkl_libs})
  find_library(SCALAPACK_${s}_LIBRARY
           NAMES ${s}
           PATHS ENV MKLROOT ENV I_MPI_ROOT ENV TBBROOT
           PATH_SUFFIXES
             lib/intel64 lib/intel64_win
             intel64/lib/release
             lib/intel64/gcc4.7 ../tbb/lib/intel64/gcc4.7
             lib/intel64/vc_mt ../tbb/lib/intel64/vc_mt
             ../compiler/lib/intel64
           HINTS ${MKL_LIBRARY_DIRS} ${MKL_LIBDIR}
           NO_DEFAULT_PATH)
  if(NOT SCALAPACK_${s}_LIBRARY)
    message(WARNING "MKL component not found: " ${s})
    return()
  endif()

  list(APPEND SCALAPACK_LIB ${SCALAPACK_${s}_LIBRARY})
endforeach()


find_path(SCALAPACK_INCLUDE_DIR
  NAMES mkl_scalapack.h
  PATHS ENV MKLROOT ENV I_MPI_ROOT ENV TBBROOT
  PATH_SUFFIXES
    include
    include/intel64/${_mkl_bitflag}lp64
  HINTS ${MKL_INCLUDE_DIRS})

if(NOT SCALAPACK_INCLUDE_DIR)
  message(WARNING "MKL Include Dir not found")
  return()
endif()

list(APPEND SCALAPACK_INCLUDE_DIR
  ${MKL_INCLUDE_DIRS})

set(SCALAPACK_LIBRARY ${SCALAPACK_LIB} PARENT_SCOPE)
set(SCALAPACK_INCLUDE_DIR ${SCALAPACK_INCLUDE_DIR} PARENT_SCOPE)

endfunction(mkl_scala)

#==== main program

cmake_policy(VERSION 3.11)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
  cmake_policy(SET CMP0074 NEW)
  cmake_policy(SET CMP0075 NEW)
endif()

if(NOT (OpenMPI IN_LIST SCALAPACK_FIND_COMPONENTS
        OR MPICH IN_LIST SCALAPACK_FIND_COMPONENTS
        OR MKL IN_LIST SCALAPACK_FIND_COMPONENTS))
  if(DEFINED ENV{MKLROOT})
    if(APPLE)
      list(APPEND SCALAPACK_FIND_COMPONENTS MKL MPICH)
    else()
      list(APPEND SCALAPACK_FIND_COMPONENTS MKL IntelMPI)
    endif()
  else()
    list(APPEND SCALAPACK_FIND_COMPONENTS OpenMPI)
  endif()
endif()

find_package(PkgConfig)

if(NOT WIN32)
  find_package(Threads)  # not required--for example Flang
endif()

if(MKL IN_LIST SCALAPACK_FIND_COMPONENTS)

  if(BUILD_SHARED_LIBS)
    set(_mkltype dynamic)
  else()
    set(_mkltype static)
  endif()

  if(MKL64 IN_LIST SCALAPACK_FIND_COMPONENTS)
    set(_mkl_bitflag i)
  else()
    set(_mkl_bitflag)
  endif()


  if(WIN32)
    set(_impi impi)
  else()
    unset(_impi)
  endif()


  pkg_check_modules(MKL mkl-${_mkltype}-${_mkl_bitflag}lp64-iomp)

  if(OpenMPI IN_LIST SCALAPACK_FIND_COMPONENTS)
    mkl_scala(mkl_scalapack_${_mkl_bitflag}lp64 mkl_blacs_openmpi_${_mkl_bitflag}lp64)
    if(SCALAPACK_LIBRARY)
      set(SCALAPACK_OpenMPI_FOUND true)
    endif()
  elseif(MPICH IN_LIST SCALAPACK_FIND_COMPONENTS)
    if(APPLE)
      mkl_scala(mkl_scalapack_${_mkl_bitflag}lp64 mkl_blacs_mpich_${_mkl_bitflag}lp64)
    elseif(WIN32)
       mkl_scala(mkl_scalapack_${_mkl_bitflag}lp64 mkl_blacs_mpich2_${_mkl_bitflag}lp64.lib mpi.lib fmpich2.lib)
    else()  # linux, yes it's just like IntelMPI
       mkl_scala(mkl_scalapack_${_mkl_bitflag}lp64 mkl_blacs_intelmpi_${_mkl_bitflag}lp64)
    endif()
    if(SCALAPACK_LIBRARY)
      set(SCALAPACK_MPICH_FOUND true)
    endif()
  else()  # IntelMPI
    mkl_scala(mkl_scalapack_${_mkl_bitflag}lp64 mkl_blacs_intelmpi_${_mkl_bitflag}lp64 ${_impi})
    if(SCALAPACK_LIBRARY)
      set(SCALAPACK_IntelMPI_FOUND true)
    endif()
  endif()

  if(SCALAPACK_LIBRARY)
    set(SCALAPACK_MKL_FOUND true)
  endif()

elseif(OpenMPI IN_LIST SCALAPACK_FIND_COMPONENTS)

  pkg_check_modules(SCALAPACK scalapack-openmpi)

  find_library(SCALAPACK_LIBRARY
               NAMES scalapack scalapack-openmpi
               HINTS ${SCALAPACK_LIBRARY_DIRS} ${SCALAPACK_LIBDIR})

  if(SCALAPACK_LIBRARY)
    set(SCALAPACK_OpenMPI_FOUND true)
  endif()

elseif(MPICH IN_LIST SCALAPACK_FIND_COMPONENTS)
  find_library(SCALAPACK_LIBRARY
               NAMES scalapack-mpich scalapack-mpich2)

  if(SCALAPACK_LIBRARY)
    set(SCALAPACK_MPICH_FOUND true)
  endif()

endif()

# Finalize

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  SCALAPACK
  REQUIRED_VARS SCALAPACK_LIBRARY
  HANDLE_COMPONENTS)

if(SCALAPACK_FOUND)
  set(SCALAPACK_LIBRARIES ${SCALAPACK_LIBRARY})
  set(SCALAPACK_INCLUDE_DIRS ${SCALAPACK_INCLUDE_DIR})
endif()

mark_as_advanced(SCALAPACK_LIBRARY SCALAPACK_INCLUDE_DIR)
