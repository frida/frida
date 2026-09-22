project('A', 'c')

# Same as the previous test but use C and B in
# the opposite order.

C = subproject('C')
c = C.get_variable('c')

B = subproject('B')
b = B.get_variable('b')

a = executable('a', 'a.c', link_with : [b, c])
test('a test', a)
