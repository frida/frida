project('b', 'c')

c_dep = dependency('c', version:'2', fallback: ['c', 'c_dep'])

blib = library('b', 'b.c',
  dependencies: c_dep)

b_dep = declare_dependency(
  link_with: blib,
  include_directories: include_directories('.'),
)
