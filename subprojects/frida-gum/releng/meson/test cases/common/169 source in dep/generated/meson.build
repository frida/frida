fp = find_program('genheader.py')

genh = custom_target('genh',
  input : 'funname',
  output : 'funheader.h',
  command : [fp, '@INPUT@', '@OUTPUT@'])

dep = declare_dependency(sources : [genh])

e = executable('genuser', 'main.c',
  dependencies : dep)
test('genuser', e)
