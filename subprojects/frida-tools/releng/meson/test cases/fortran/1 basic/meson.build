project('simple fortran', 'fortran')

fc = meson.get_compiler('fortran')
if fc.get_id() == 'gcc'
  add_global_arguments('-fbounds-check', language : 'fortran')
endif

args = fc.first_supported_argument(['-ffree-form', '-free', '/free'])
assert(args != [], 'No arguments found?')

e = executable('simple', 'simple.f90',
  fortran_args : args)
test('Simple Fortran', e)
