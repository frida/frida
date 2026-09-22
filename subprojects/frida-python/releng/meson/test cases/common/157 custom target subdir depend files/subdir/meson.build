foo_src = custom_target('foo_src',
  depend_files : 'dep.dat',
  input : 'foo.c.in',
  output : 'foo.c',
  command : [copy, '@INPUT@', '@OUTPUT@']
)
