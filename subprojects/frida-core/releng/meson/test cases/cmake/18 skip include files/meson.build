project('cmakeSubTest', ['c', 'cpp'])

cm = import('cmake')

sub_pro = cm.subproject('cmMod')
sub_dep = sub_pro.dependency('cmModLib++')

exe1 = executable('main', ['main.cpp'], dependencies: [sub_dep])
test('test1', exe1)
