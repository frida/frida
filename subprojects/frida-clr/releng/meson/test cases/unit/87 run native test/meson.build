project('run native test', ['c'])

executable('target_exe', 'main.c')

native_exe = executable('native_exe', 'main.c', native: true)
test('native_exe', native_exe, args: ['native_test_has_run.stamp'])
