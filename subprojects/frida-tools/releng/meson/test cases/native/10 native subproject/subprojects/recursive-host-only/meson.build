project('recursive-host-only', 'c')

subp = subproject('recursive-build-only', native : true)

prog = subp.get_variable('exe')

ct = custom_target(
  'main.c',
  output : 'main.c',
  command : prog,
  capture : true,
)

exe = executable('main', ct, native : true)

test('main', exe)
