st1 = static_library('st1', 'lib1.c', 'prop1.c')
st2 = static_library('st2', 'lib2.c', 'prop2.c')
st3 = static_library('st3', 'lib3.c', 'prop3.c')

test('circular', executable('circular', 'main.c', link_with : [st1, st2, st3]))
