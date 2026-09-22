project('A', 'c', subproject_dir:'custom_subproject_dir')

B = subproject('B')
b = B.get_variable('b')

C = subproject('C')
c = C.get_variable('c')

subdir('other_subdir')

a = executable('a', 'a.c', link_with : [b, c])
test('a test', a)
