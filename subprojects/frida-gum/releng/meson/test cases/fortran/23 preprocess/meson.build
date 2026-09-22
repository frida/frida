project('preprocess', 'fortran')

fc = meson.get_compiler('fortran')

pp_files = fc.preprocess('main.f90', output: '@PLAINNAME@')

library('foo', pp_files)
