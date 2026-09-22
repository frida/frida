# Define version only in project, should get inherited by declare_dependency
project('some', 'c', version : '0.1')

somelib = library('some', 'lib.c')
someinc = include_directories('.')

some_dep = declare_dependency(link_with : somelib,
  include_directories : someinc)

fakezlib_dep = declare_dependency(link_with : somelib,
  include_directories : someinc)
