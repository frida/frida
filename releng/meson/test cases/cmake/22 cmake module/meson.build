project('cmakeModule', 'c', version: '1.0.0')

if build_machine.system() == 'cygwin'
  error('MESON_SKIP_TEST CMake is broken on Cygwin.')
endif

cmake_bin = find_program('cmake', required: false)
if not cmake_bin.found()
  error('MESON_SKIP_TEST CMake not installed.')
endif

cc = meson.get_compiler('c')
if cc.get_id() == 'clang-cl' and meson.backend() == 'ninja' and build_machine.system() == 'windows'
    error('MESON_SKIP_TEST CMake installation nor operational for vs2017 clangclx64ninja')
endif

cmake = import('cmake')

cmake.write_basic_package_version_file(version: '0.0.1',
   name: 'cmakeModule',
)

conf = configuration_data()
conf.set('MYVAR', 'my variable value')
conf.set_quoted('MYQUOTEDVAR', 'my quoted variable value')

cmake.configure_package_config_file(
    input: 'projectConfig.cmake.in',
    name: 'cmakeModule',
    configuration: conf,
)
