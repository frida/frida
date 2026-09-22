project('exe wrapper behaviour', 'c')

assert(meson.is_cross_build(), 'not setup as cross build')
assert(meson.has_exe_wrapper(), 'exe wrapper not defined?')  # intentionally not changed to can_run_host_binaries,

exe = executable('prog', 'prog.c')

if get_option('custom-target')
  custom_target('use-exe-wrapper',
    build_by_default: true,
    output: 'out.txt',
    command: [exe, '@OUTPUT@'])
endif

test('test-prog', exe)

if get_option('run-target')
  run_target('run-prog', command : exe)
endif
