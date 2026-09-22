deps = [dependency('threads'), cc.find_library('dl'), cc.find_library('m')]

liba = library('a', 'liba.c',
  dependencies: deps)

liba_dep = declare_dependency(link_with: liba,
  include_directories: include_directories('.'),
  dependencies: deps)
