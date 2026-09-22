project('cmakeSubTest', ['c', 'cpp'])

cm = import('cmake')

sub_pro = cm.subproject('cmMod')
sub_dep = sub_pro.dependency('cmModLib')

assert(sub_pro.target_list() == ['cmModLib'], 'There should be exactly one target')
assert(sub_pro.target_type('cmModLib') == 'header_only', 'Target type should be header_only')

exe1 = executable('main', ['main.cpp'], dependencies: [sub_dep])
test('test1', exe1)
