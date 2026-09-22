project(
  'meson_cmake_system_include_bug', ['c', 'cpp'],
  default_options: [
    'warning_level=3',
    'werror=true',
  ],
)

if meson.get_compiler('cpp').get_argument_syntax() == 'msvc'
  error('MESON_SKIP_TEST: Skip with msvc due to missing -system support')
endif

cm = import('cmake')
sub_pro = cm.subproject('cmMod')
sub_dep = sub_pro.dependency('cmModLib')

exe1 = executable('main1', ['main.cpp'], dependencies: [sub_dep])
test('test1', exe1)
