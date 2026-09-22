project('compiles_links_runs', 'fortran')

fc = meson.get_compiler('fortran')

code = '''error stop 123; end'''

if not fc.compiles(code)
  error('Fortran 2008 code failed to compile')
endif

if not fc.links(code)
  error('Fortran 2008 code failed to link')
endif

if fc.run(code).returncode() != 123
  error('Fortran 2008 code failed to run')
endif
