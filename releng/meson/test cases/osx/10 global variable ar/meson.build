# Source: https://lists.gnu.org/archive/html/libtool/2002-07/msg00025.html

project('global variable test', 'c')

lib = static_library('mylib', 'libfile.c', 'libfile2.c')
test('global variable', executable('prog', 'prog.c', link_with: lib))
