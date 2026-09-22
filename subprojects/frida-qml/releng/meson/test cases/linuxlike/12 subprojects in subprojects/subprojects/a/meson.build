project('a', 'c', version:'1.0')

c_dep = dependency('c', version:'1', fallback: ['c', 'c_dep'])

alib = library('a', 'a.c',
  dependencies: c_dep)

a_dep = declare_dependency(
  link_with: alib,
  include_directories: include_directories('.'),
)
