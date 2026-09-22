project('doxygen test', 'cpp', version : '0.1.0', meson_version: '>=0.56')

spede_inc = include_directories('include')

spede_src = [ 'src/spede.cpp' ]

spede_lib = library('spede', spede_src, include_directories: spede_inc)

doxygen = find_program('doxygen', required : false)
if not doxygen.found()
  error('MESON_SKIP_TEST doxygen not found.')
endif

cdata = configuration_data()
cdata.set('VERSION', meson.project_version())

if find_program('dot', required : false).found()
  # In the real world this would set the variable
  # to YES. However we set it to NO so that the
  # list of generated files is always the same
  # so tests always pass.
  cdata.set('HAVE_DOT', 'NO')
else
  cdata.set('HAVE_DOT', 'NO')
endif

subdir('doc')
