project('dist script', 'c',
  version : '1.0.0')

exe = executable('comparer', 'prog.c')
test('compare', exe)

meson.add_dist_script('replacer.py', '"incorrect"', '"correct"')
meson.add_dist_script(find_program('replacer.py'), '"incorrect"', '"correct"')

subproject('sub')
