project('s1', 'c')

sc = subproject('scommon')
s3 = subproject('s3')

executable('s1', 's1.c',
  link_with : [sc.get_variable('clib'), s3.get_variable('l')])
