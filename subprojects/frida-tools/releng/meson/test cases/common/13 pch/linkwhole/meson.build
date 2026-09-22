# https://github.com/mesonbuild/meson/issues/10745

l2 = static_library('two', 'lib2.c', c_pch: 'pch2/pch_two.h')
l1 = static_library('one', 'lib1.c', c_pch: 'pch1/pch_one.h',
  link_whole: l2)

executable('linkprog', 'main.c',
  link_with: l1)
