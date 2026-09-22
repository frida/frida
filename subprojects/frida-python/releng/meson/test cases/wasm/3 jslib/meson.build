project('jslib', 'c')

cc = meson.get_compiler('c')

sf_dep = cc.find_library('somefuncs.js', dirs: meson.current_source_dir())

executable('libcallc', 'prog.c',
    dependencies: sf_dep)