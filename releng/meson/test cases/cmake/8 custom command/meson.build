project('cmakeSubTest', ['c', 'cpp'])

if meson.is_cross_build()
  error('MESON_SKIP_TEST this test does not cross compile correctly.')
endif

cm = import('cmake')

sub_pro = cm.subproject('cmMod')
sub_dep = sub_pro.dependency('cmModLib')

assert(sub_pro.target_type('cmModLib') == 'shared_library', 'Target type should be shared_library')
assert(sub_pro.target_type('gen') == 'executable', 'Target type should be executable')

exe1 = executable('main', ['main.cpp'], dependencies: [sub_dep])
test('test1', exe1)
