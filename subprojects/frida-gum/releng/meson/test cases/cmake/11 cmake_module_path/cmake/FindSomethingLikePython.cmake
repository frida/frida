cmake_policy(VERSION 3.7)

find_package(Python COMPONENTS Interpreter)
if(Python_FOUND OR PYTHONINTERP_FOUND)
  set(SomethingLikePython_FOUND      ON)
  set(SomethingLikePython_EXECUTABLE ${Python_EXECUTABLE})
else()
  set(SomethingLikePython_FOUND OFF)
endif()
