project('Inclusive', 'fortran',
  meson_version: '>= 0.51.1')

cm = import('cmake')

hier_exe = executable('include_hierarchy', 'include_hierarchy.f90')
test('Fortran include file hierarchy', hier_exe)

syntax_exe = executable('include_syntax', 'include_syntax.f90')
test('Fortran include file syntax', syntax_exe)

# older CI runs into problems with too-old Ninja and CMake and Fortran
ninja_version = run_command('ninja', '--version', check: true).stdout().strip()
cmake_version = run_command('cmake', '--version', check: true).stdout().split()[2]
if ninja_version.version_compare('>=1.10.0') and cmake_version.version_compare('>=3.17.0')
  cm.subproject('cmake_inc')
else
  message('SKIP: CMake Fortran subproject with include. Ninja >= 1.10 and CMake >= 3.17 needed. You have Ninja ' + ninja_version + ' and CMake ' + cmake_version)
endif
