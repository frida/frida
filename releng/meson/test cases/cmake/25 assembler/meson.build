project('assembler test', ['c', 'cpp'])

cm = import('cmake')

sub_pro = cm.subproject('cmTest')
sub_dep = sub_pro.dependency('cmTest')

exe1 = executable('exe1', ['main.c'], dependencies: [sub_dep])
test('test1', exe1)
