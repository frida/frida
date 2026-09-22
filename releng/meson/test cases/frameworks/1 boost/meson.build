# this test requires the following on Ubuntu: libboost-{system,python,log,thread,test}-dev
project('boosttest', 'cpp',
  default_options : ['cpp_std=c++14'])

s = get_option('static')

dep = dependency('boost', static: s, required: false)
if not dep.found()
  error('MESON_SKIP_TEST boost not found.')
endif

# We want to have multiple separate configurations of Boost
# within one project. The need to be independent of each other.
# Use one without a library dependency and one with it.

linkdep     = dependency('boost', static: s, modules : ['thread', 'system', 'date_time'])
testdep     = dependency('boost', static: s, modules : ['unit_test_framework'])
nomoddep    = dependency('boost', static: s)
extralibdep = dependency('boost', static: s, modules : ['thread', 'system', 'date_time', 'log_setup', 'log', 'filesystem', 'regex'])
notfound    = dependency('boost', static: s, modules : ['this_should_not_exist_on_any_system'], required: false)

assert(not notfound.found())

require_bp = host_machine.system() in ['linux', 'darwin']
pymod = import('python')
python2 = pymod.find_installation('python2', required: false                           , disabler: true)
python3 = pymod.find_installation('python3', required: require_bp , disabler: true)
python2dep = python2.dependency(required: false                           , embed: true, disabler: true)
python3dep = python3.dependency(required: require_bp, embed: true, disabler: true)

# compile python 2/3 modules only if we found a corresponding python version
if(python2dep.found() and require_bp and not s)
  bpython2dep = dependency('boost', static: s, modules : ['python'], required: false, disabler: true)
else
  python2dep = disabler()
  bpython2dep = disabler()
endif

if(python3dep.found() and require_bp and not s)
  bpython3dep = dependency('boost', static: s, modules : ['python3'])
else
  python3dep = disabler()
  bpython3dep = disabler()
endif

linkexe = executable('linkedexe', 'linkexe.cc', dependencies : linkdep)
unitexe = executable('utf', 'unit_test.cpp', dependencies: testdep)
nomodexe = executable('nomod', 'nomod.cpp', dependencies : nomoddep)
extralibexe = executable('extralibexe', 'extralib.cpp', dependencies : extralibdep)

# python modules are shared libraries
python2module = python2.extension_module('python2_module', ['python_module.cpp'], dependencies: [python2dep, bpython2dep], cpp_args: ['-DMOD_NAME=python2_module'])
python3module = python3.extension_module('python3_module', ['python_module.cpp'], dependencies: [python3dep, bpython3dep], cpp_args: ['-DMOD_NAME=python3_module'])


test('Boost linktest', linkexe, timeout: 60)
test('Boost UTF test', unitexe, timeout: 60)
test('Boost nomod', nomodexe)
if host_machine.system() != 'darwin' or s
  # Segfaults on macOS with dynamic linking since Boost 1.73
  # https://github.com/mesonbuild/meson/issues/7535
  test('Boost extralib test', extralibexe)
endif

# explicitly use the correct python interpreter so that we don't have to provide two different python scripts that have different shebang lines
python2interpreter = find_program(python2.path(), required: false, disabler: true)
test('Boost Python2', python2interpreter, args: ['./test_python_module.py', meson.current_build_dir()], workdir: meson.current_source_dir(), depends: python2module)
python3interpreter = find_program(python3.path(), required: false, disabler: true)
test('Boost Python3', python3interpreter, args: ['./test_python_module.py', meson.current_build_dir()], workdir: meson.current_source_dir(), depends: python3module)

subdir('partial_dep')

# check we can apply a version constraint
dependency('boost', static: s, version: '>=@0@'.format(dep.version()))
