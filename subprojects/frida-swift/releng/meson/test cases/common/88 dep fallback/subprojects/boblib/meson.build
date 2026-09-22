project('bob', 'c')

gensrc_py = find_program('genbob.py')
genbob_h = custom_target('genbob.h',
  output : 'genbob.h',
  command : [gensrc_py, '@OUTPUT@'])
genbob_c = custom_target('genbob.c',
  output : 'genbob.c',
  command : [gensrc_py, '@OUTPUT@'])

boblib = library('bob', ['bob.c', genbob_c])
bobinc = include_directories('.')

bob_dep = declare_dependency(link_with : boblib,
  sources : [genbob_h],
  include_directories : bobinc)

notfound_dep = dependency('', required: false)
