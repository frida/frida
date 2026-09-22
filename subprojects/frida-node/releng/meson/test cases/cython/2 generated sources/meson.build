project(
  'generated cython sources',
  ['cython', 'c'],
  default_options : ['buildtype=release'],
)

if meson.backend() != 'ninja'
  error('MESON_SKIP_TEST: Ninja backend required')
endif

fs = import('fs')
py3 = import('python').find_installation('python3')
py3_dep = py3.dependency(required : false)
if not py3_dep.found()
  error('MESON_SKIP_TEST: Python library not found.')
endif

ct = custom_target(
  'ct',
  input : 'gen.py',
  output : 'ct.pyx',
  command : [py3, '@INPUT@', '@OUTPUT@'],
)

ct_ext = py3.extension_module('ct', ct)

test(
  'custom target',
  py3,
  args : [files('test.py'), 'ct'],
  env : ['PYTHONPATH=' + meson.current_build_dir()]
)

# Test a CustomTargetIndex
cti = custom_target(
  'cti',
  input : 'gen.py',
  output : 'cti.pyx',
  command : [py3, '@INPUT@', '@OUTPUT@'],
)

cti_ext = py3.extension_module('cti', cti[0])

cf = configure_file(
  input : 'configure.pyx.in',
  output : 'cf.pyx',
  copy : true,
)

cf_ext = py3.extension_module('cf', cf)

test(
  'configure file',
  py3,
  args : [files('test.py'), 'cf'],
  env : ['PYTHONPATH=' + meson.current_build_dir()]
)

gen = generator(
  find_program('generator.py'),
  arguments : ['@INPUT@', '@OUTPUT@'],
  output : '@BASENAME@.pyx',
)

g_ext = py3.extension_module(
  'g',
  gen.process('g.in'),
)

test(
  'generator',
  py3,
  args : [files('test.py'), 'g'],
  env : ['PYTHONPATH=' + meson.current_build_dir()]
)

stuff_pxi = fs.copyfile(
  'stuff.pxi.in',
  'stuff.pxi'
)

# Need to copy the cython source to the build directory
# since meson can only generate the .pxi there
includestuff_pyx = fs.copyfile(
  'includestuff.pyx'
)

stuff_pxi_dep = declare_dependency(sources: stuff_pxi)

includestuff_ext = py3.extension_module(
  'includestuff',
  includestuff_pyx,
  dependencies: stuff_pxi_dep
)

simpleinclude_ext = py3.extension_module(
  'simpleinclude',
  'simpleinclude.pyx',
)

subdir('libdir')

test(
  'custom target in subdir',
  py3,
  args : [files('test.py'), 'ct2'],
  env : ['PYTHONPATH=' + pydir]
)
