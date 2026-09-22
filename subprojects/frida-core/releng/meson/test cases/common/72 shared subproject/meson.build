project('A', 'c')

B = subproject('B')
b = B.get_variable('b')

C = subproject('C')
c = C.get_variable('c')

a = executable('a', 'a.c', link_with : [b, c])
test('a test', a)
