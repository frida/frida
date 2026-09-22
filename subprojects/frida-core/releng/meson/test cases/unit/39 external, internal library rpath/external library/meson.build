project('system library', 'c', default_options : ['b_lundef=false'])

shared_library('foo_in_system', 'foo.c', install : true)
l = shared_library('faa_pkg', 'faa.c', install: true)

if host_machine.system() == 'darwin'
  ldflags = ['-framework', 'CoreFoundation', '-framework', 'CoreMedia']
  allow_undef_args = ['-Wl,-undefined,dynamic_lookup']
else
  ldflags = ['-Wl,-rpath,${libdir}']
  allow_undef_args = []
endif

pkg = import('pkgconfig')
pkg.generate(name: 'faa_pkg',
             libraries: [l] + ldflags,
             description: 'FAA, a pkg-config test library')

# cygwin DLLs can't have undefined symbols
if host_machine.system() != 'cygwin'
  shared_library('bar_in_system', 'bar.c', install : true, link_args : allow_undef_args)
endif
