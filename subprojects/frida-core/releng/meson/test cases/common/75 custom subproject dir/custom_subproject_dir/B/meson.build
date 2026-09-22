project('B', 'c')
C = subproject('C')
c = C.get_variable('c')
b = shared_library('b', 'b.c', link_with : c)
