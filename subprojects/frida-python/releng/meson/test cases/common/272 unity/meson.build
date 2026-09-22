project('unity', 'c',
  default_options : [
    'unity_size=2'])

if get_option('unity') != 'on'
  error('MESON_SKIP_TEST: unity builds not enabled')
endif

slib_notinstalled = static_library('slib_notinstalled',
  # test depends on the number of files being divisible by unity_size
  ['slib1.c', 'slib2.c'])

slib_installed = static_library('slib_installed', ['slib1.c', 'slib2.c'], link_with : slib_notinstalled, install : true)
