project('a', 'c')

b = subproject('b')
l = shared_library('a', 'a.c', link_with : b.get_variable('lb'))
