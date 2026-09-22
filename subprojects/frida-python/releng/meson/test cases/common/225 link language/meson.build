project(
  'link_language',
  ['c', 'cpp'],
)

exe = executable(
  'main',
  ['main.c', 'c_linkage.cpp'],
  link_language : 'c',
)

lib = library(
  'mylib',
  ['lib.cpp'],
  link_language : 'c',
)

test('main', exe)
