project('Python extension module', 'c',
  default_options : ['buildtype=release'])

py_mod = import('python')

py = py_mod.find_installation(get_option('python'), required : false)

# CI images don't have 32-bit python2 for 32-bit windows,
# so this actually gets detected then fails
require = not (
  get_option('python') == 'python2' and
  host_machine.system() == 'windows' and
  host_machine.cpu() == 'x86'
)

if py.found()
  py_dep = py.dependency(required: require)

  if py_dep.found()
    subdir('ext')

    test('extmod',
      py,
      args : files('blaster.py'),
      env : ['PYTHONPATH=' + pypathdir])
  else
    error('MESON_SKIP_TEST: Python libraries not found, skipping test.')
  endif
else
  error('MESON_SKIP_TEST: Python not found, skipping test.')
endif

py = py_mod.find_installation(get_option('python'), required : get_option('disabled_opt'))
assert(not py.found(), 'find_installation not working with disabled feature')
