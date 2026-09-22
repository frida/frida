project('netcdf_test', 'c')

cc = meson.get_compiler('c')
c_code = '''#include <netcdf.h>
int main(void) { return 0; }'''
# --- C
nc_c = dependency('netcdf', language : 'c', required : false, disabler: true)
if not cc.links(c_code, dependencies: nc_c, name: 'NetCDF C')
  nc_c = disabler()
endif
if not nc_c.found()
  error('MESON_SKIP_TEST: NetCDF C library not found, skipping NetCDF framework tests.')
endif
exec = executable('exec', 'main.c', dependencies : nc_c)
test('NetCDF C', exec, timeout: 15)

# --- C++
if add_languages('cpp', required: false)
  nc_cpp = dependency('netcdf', language : 'cpp', required : false, disabler: true)
  execpp = executable('execpp', 'main.cpp', dependencies : nc_cpp)
  test('NetCDF C++', execpp, timeout: 15)
endif

# --- Fortran
if build_machine.system() != 'windows' and build_machine.system() != 'darwin'
  if add_languages('fortran', required: false)
    nc_f = dependency('netcdf', language : 'fortran', required : false, disabler: true)
    exef = executable('exef', 'main.f90', dependencies : nc_f)
    test('NetCDF Fortran', exef, timeout: 15)
  endif
endif

# Check we can apply a version constraint
if nc_c.version() != 'unknown'
  dependency('netcdf', version: '>=@0@'.format(nc_c.version()))
endif
