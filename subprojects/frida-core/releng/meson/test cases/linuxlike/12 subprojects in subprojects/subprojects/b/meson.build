project('b', 'c')

c_dep = dependency('c', version:'2', fallback: ['c', 'c_dep'], required: false)

assert(c_dep.found() == false, 'C project has the wrong version and should not be found')

if c_dep.found()
    add_global_arguments('-DWITH_C', language: 'c')
endif

blib = library('b', 'b.c',
  dependencies: c_dep)

b_dep = declare_dependency(
  link_with: blib,
  include_directories: include_directories('.'),
)
