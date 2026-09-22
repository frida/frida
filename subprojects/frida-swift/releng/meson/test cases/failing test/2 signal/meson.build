project('signal', 'c')

if build_machine.system() == 'windows'
    error('MESON_SKIP_TEST test is not compatible with MS Windows.')
else
    test('My Signal Test', executable('main', 'main.c'))
endif
