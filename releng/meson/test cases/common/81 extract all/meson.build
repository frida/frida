project('extract all', 'c')

a = static_library('a', 'one.c', 'two.c')
b = static_library('b', 'three.c', 'four.c')
c = static_library('c', objects : [a.extract_all_objects(), b.extract_all_objects()])
d = static_library('d', objects : [a.extract_all_objects(), b.extract_all_objects(), c.extract_all_objects()])
d_recursive = static_library('d_recursive', objects : [c.extract_all_objects(recursive : true)])

e = executable('proggie', 'prog.c', link_with : d)
test('extall', e)

e = executable('proggie_recursive', 'prog.c', link_with : d_recursive)
test('extall_recursive', e)
