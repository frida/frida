project('some', 'c')

somelib = library('some', 'lib.c')
someinc = include_directories('.')

# Define version only in declare_dependency
some_dep = declare_dependency(link_with : somelib,
  include_directories : someinc)
