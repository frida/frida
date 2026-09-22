project('s2', 'c')

sc = subproject('scommon')

executable('s2', 's2.c', link_with : sc.get_variable('clib'))
