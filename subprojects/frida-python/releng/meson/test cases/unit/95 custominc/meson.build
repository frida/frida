project('implicit custom dirs', 'c')

subdir('easytogrepfor')

l = static_library('helper', 'helper.c', genh)
d = declare_dependency(link_with: l, sources: genh)
executable('prog', 'prog.c', dependencies: d, implicit_include_directories: false)

executable('prog2', 'prog2.c', dependencies: d)
