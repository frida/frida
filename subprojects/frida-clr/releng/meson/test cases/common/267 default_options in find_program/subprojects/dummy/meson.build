project('dummy', 'c')

if get_option('subproject_option')
    dummy_exe = executable('dummy', 'dummy.c')
    meson.override_find_program('dummy', dummy_exe)
endif
