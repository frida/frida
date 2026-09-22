project(
  'threads',
  'c', 'cpp',
  default_options : ['cpp_std=c++11'],
)

dep_threads = dependency('threads')

executable('threads-c', 'threads.c', dependencies : dep_threads)
executable('threads-c++', 'threads.cpp', dependencies : dep_threads)
