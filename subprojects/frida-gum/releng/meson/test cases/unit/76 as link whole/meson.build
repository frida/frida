project('as-link-whole', 'c')

foo = static_library('foo', 'foo.c', install: true)
dep = declare_dependency(link_with: foo)
bar1 = library('bar1', 'bar.c', dependencies: dep)
bar2 = library('bar2', 'bar.c', dependencies: dep.as_link_whole())

# bar1.pc should have -lfoo but not bar2.pc
pkg = import('pkgconfig')
pkg.generate(bar1)
pkg.generate(bar2)
