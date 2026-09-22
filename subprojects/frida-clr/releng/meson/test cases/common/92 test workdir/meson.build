project('test workdir', 'c')

exe = executable('opener', 'opener.c')

test('basic', exe, workdir : meson.source_root())
test('shouldfail', exe, should_fail : true)

subdir('subdir')
