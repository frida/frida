mod = import('unstable-external_project')

target_system = target_machine.system()
if target_system in ['windows', 'cygwin']
  libext = 'dll'
elif target_system == 'darwin'
  libext = 'dylib'
else
  libext = 'so'
endif

p = mod.add_project('configure',
  configure_options : [
    '--prefix=@PREFIX@',
    '--libdir=@PREFIX@/@LIBDIR@',
    '--includedir=@PREFIX@/@INCLUDEDIR@',
    '--libext=' + libext,
  ],
  depends: somelib,
)

libfoo_dep = declare_dependency(link_with : somelib,
  dependencies : p.dependency('foo'))
