cmake_minimum_required(VERSION 3.6.0)
project(dylibversion C)

# This file is here for debugging purposes to easily compare how
# CMake does it.

# libnoversion.dylib
add_library(noversion SHARED lib.c)

# libonlysoversion.dylib -> libonlysoversion.5.dylib
# libonlyversion.1.4.5.dylib
# -current_version 1.4.5

add_library(onlyversion SHARED lib.c)
set_target_properties(onlyversion PROPERTIES VERSION 1.4.5)

# libonlysoversion.6.dylib
# -compatibility_version 6.0.0

add_library(onlysoversion SHARED lib.c)
set_target_properties(onlysoversion PROPERTIES SOVERSION 6)

# libsome.1.4.5.dylib
# libsome.6.dylib -> libsome.1.4.5.dylib
# libsome.dylib -> libsome.6.dylib
# -current_version 1.4.5 -compatibility_version 5.0.0

add_library(some SHARED lib.c)
set_target_properties(some PROPERTIES VERSION 1.4.5 SOVERSION 6)
