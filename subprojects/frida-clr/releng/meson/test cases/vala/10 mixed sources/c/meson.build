writec = find_program('writec.py')

retval = custom_target('writec',
  output : 'retval.c',
  command : [writec, '@OUTPUT@'])
