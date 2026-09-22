project('nobuilddir', 'c',
  default_options : ['werror=true', 'buildtype=plain'])

cc = meson.get_compiler('c')

incwarg = '-Wmissing-include-dirs'

if cc.has_argument(incwarg)
  executable('prog', 'prog.c',
    c_args : incwarg,
    include_directories : include_directories('include'))
else
  error('MESON_SKIP_TEST compiler does not support bad inc dir argument.')
endif
