project('generator host binary no exe_wrapper')

if meson.can_run_host_binaries()
  error('MESON_SKIP_TEST: test requires that build machine cannot run host binaries')
endif

add_languages('c', native : false)

exe = executable('exe', 'exe.c', native : false)

gen = generator(exe, output : '@BASENAME@.c', arguments : ['@INPUT@', '@OUTPUT@'])
foo = gen.process('lib.in')

library('foo', foo)
