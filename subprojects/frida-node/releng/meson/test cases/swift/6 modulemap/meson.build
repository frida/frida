project('mixed', 'c', 'swift')

i = include_directories('.')
lib = static_library('mylib', 'mylib.c')
exe = executable('prog', 'main.swift',
  include_directories : [i],
  link_with : lib)
test('c module', exe)
