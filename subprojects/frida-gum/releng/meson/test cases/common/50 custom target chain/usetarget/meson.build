e = executable('myexe', 'myexe.c')
subexe = find_program('subcomp.py')

custom_target('use_exe',
  input : e,
  output : 'subout.res',
  command : [subexe, '@INPUT@', '@OUTPUT@'],
)
