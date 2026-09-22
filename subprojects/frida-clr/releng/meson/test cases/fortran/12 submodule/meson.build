project('submodule single level', 'fortran',
  meson_version: '>= 0.50.0')

fortc = meson.get_compiler('fortran')
if fortc.get_id() == 'gcc' and fortc.version().version_compare('<6.0')
  error('MESON_SKIP_TEST need gfortran >= 6.0 for submodule support')
endif

hier2 = executable('single', 'parent.f90', 'child.f90')
test('single-level hierarchy', hier2)

hierN = executable('multi', 'a1.f90', 'a2.f90', 'a3.f90')
test('multi-level hierarchy', hierN)
