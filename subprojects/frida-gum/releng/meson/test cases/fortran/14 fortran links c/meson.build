project('Fortran calling C', 'fortran', 'c',
  meson_version: '>= 0.51.0',
  default_options : ['default_library=static'])

ccid = meson.get_compiler('c').get_id()
fcid = meson.get_compiler('fortran').get_id()

if fcid == 'gcc' and ccid in ['msvc', 'clang-cl']
  error('MESON_SKIP_TEST: MSVC and GCC do not interoperate like this.')
endif

c_lib = library('clib', 'clib.c', vs_module_defs : 'clib.def')

f_call_c = executable('f_call_c', 'f_call_c.f90',
  link_with: c_lib,
  link_language: 'fortran')
test('Fortran calling C', f_call_c)
