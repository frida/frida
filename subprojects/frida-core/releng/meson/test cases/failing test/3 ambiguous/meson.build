project('ambiguous', 'c')

if build_machine.system() == 'windows'
    error('MESON_SKIP_TEST test is not compatible with MS Windows.')
else
    exe = executable('main', 'main.c')
    test_runner = find_program('test_runner.sh')

    test('My Ambiguous Status Test', test_runner, args : [exe.full_path()])
endif
