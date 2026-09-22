project('build rpath', 'c', 'cpp')

subdir('sub')
executable('prog', 'prog.c',
  link_with : l,
  build_rpath : '/foo/bar',
  install_rpath : '/baz',
  install : true,
  )

executable('progcxx', 'prog.cc',
  link_with : l,
  build_rpath : '/foo/bar',
  install_rpath : 'baz',
  install : true,
  )
