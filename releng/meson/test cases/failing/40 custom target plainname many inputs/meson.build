project('plain name many inputs')

catfiles = find_program('catfiles.py')

custom_target('plainname-inputs',
  input : ['1.txt', '2.txt'],
  output : '@PLAINNAME@.dat',
  command : [catfiles, '@INPUT@', '@OUTPUT@'])
