project('build rpath', 'c', 'cpp')

subdir('sub')
pkgconf_dep = dependency('dummy')

executable('prog', 'prog.c',
  dependencies : pkgconf_dep,
  link_with : l,
  build_rpath : '/foo/bar',
  install_rpath : '/baz',
  install : true,
  )

executable('progcxx', 'prog.cc',
  dependencies : pkgconf_dep,
  link_with : l,
  build_rpath : '/foo/bar',
  install_rpath : 'baz',
  install : true,
  )
