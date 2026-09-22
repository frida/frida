project('subproj lib install', 'c',
  version : '2.3.4',
  license : 'mylicense')

if host_machine.system() == 'cygwin'
  error('MESON_SKIP_TEST linuxlike soversions not supported on Cygwin.')
endif

# Test that the subproject library gets installed
subproject('sublib', version : '1.0.0')
