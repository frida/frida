project('foo', 'c',
  default_options : 'libdir=lib')

subdir('foo')

executable('prog', 'prog.c',
  link_with : foolib,
  install : true)
