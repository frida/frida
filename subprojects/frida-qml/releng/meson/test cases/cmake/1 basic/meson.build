project('cmakeSubTest', ['c', 'cpp'])

cm = import('cmake')

sub_pro = cm.subproject('cmMod')
sub_dep = sub_pro.dependency('cmModLib++', include_type: 'system')

assert(sub_pro.found(), 'found() method reports not found, but should be found')
assert(sub_pro.target_list() == ['cmModLib++'], 'There should be exactly one target')
assert(sub_pro.target_type('cmModLib++') == 'shared_library', 'Target type should be shared_library')
assert(sub_dep.include_type() == 'system', 'the include_type kwarg of dependency() works')

exe1 = executable('main', ['main.cpp'], dependencies: [sub_dep])
test('test1', exe1)
