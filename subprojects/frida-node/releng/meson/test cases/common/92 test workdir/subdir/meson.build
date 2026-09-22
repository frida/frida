exe2 = executable('dummy', '../opener.c')
test('subdir', find_program('checker.py'),
  workdir : meson.source_root(),
  args: [exe2])
