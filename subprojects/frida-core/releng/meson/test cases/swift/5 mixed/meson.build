project('mixed', 'c', 'swift')

lib = static_library('mylib', 'mylib.c')
exe = executable('prog', 'main.swift', 'mylib.h',
  link_with : lib)
test('c interface', exe)
