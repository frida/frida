project('dynamic_fortran', 'fortran')

fcid = meson.get_compiler('fortran').get_id()
if fcid == 'intel-cl' or (host_machine.system() == 'windows' and fcid == 'pgi')
  error('MESON_SKIP_TEST: non-Gfortran Windows Fortran compilers do not do shared libraries in a Fortran standard way')
  # !DEC$ ATTRIBUTES DLLEXPORT must be used!
  # https://software.intel.com/en-us/node/535306
  # https://www.pgroup.com/resources/docs/19.4/x86/pgi-user-guide/index.htm#lib-dynlnk-bld-dll-fort
endif

dynamic = shared_library('dynamic', 'dynamic.f90')
exe = executable('test_exe', 'main.f90', link_with : dynamic)
test('dynamic-fortran', exe)
