project('foo', 'c', 'cpp')

dep = declare_dependency(sources : 'foo.c')

executable('bar', 'bar.cpp',
  dependencies : dep)

subdir('generated')
