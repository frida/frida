project('cmake_object_lib_test', 'cpp', default_options: ['cpp_std=c++11'])

if meson.is_cross_build()
  error('MESON_SKIP_TEST this test does not cross compile correctly.')
endif

cm = import('cmake')

sub_pro = cm.subproject('cmObjLib')
sub_sha = sub_pro.dependency('lib_sha')
sub_sta = sub_pro.dependency('lib_sta')

exe_sha = executable('shared', ['main.cpp'], dependencies: [sub_sha])
exe_sta = executable('static', ['main.cpp'], dependencies: [sub_sta])

test('test1', exe_sha)
test('test1', exe_sta)
