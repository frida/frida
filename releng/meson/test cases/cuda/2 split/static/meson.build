l = static_library('clib', 'lib.cu')
exe = executable('staexe', 'main_static.cpp',
  link_with : l)
test('static Cuda test', exe)
