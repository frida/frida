project('run command', version : run_command('get-version.py', check : true).stdout().strip(), meson_version: '>=0.1.0')

if build_machine.system() == 'windows'
  c = run_command('cmd', '/c', 'echo', 'hello', check: false)
else
  c = run_command('echo', 'hello', check: false)
endif

correct = 'hello'

if c.returncode() != 0
  error('Executing echo failed.')
endif

result = c.stdout().strip()

if result != correct
  error('Getting stdout failed.')
endif

if c.stderr() != ''
  error('Extra text in stderr.')
endif

# Now the same with a script.

if build_machine.system() == 'windows'
  cs = run_command('scripts/hello.bat', check: false)
else
  cs = run_command('scripts/hello.sh', check: false)
endif

if cs.returncode() != 0
  error('Executing script failed.')
endif

if cs.stdout().strip() != correct
  error('Getting stdout failed (script).')
endif

if cs.stderr() != ''
  error('Extra text in stderr (script).')
endif

# We should be able to have files() in argument
f = files('meson.build')

if build_machine.system() == 'windows'
  c = run_command('cmd', '/c', 'echo', f, check: false)
else
  c = run_command('echo', f, check: false)
endif

if c.returncode() != 0
  error('Using files() in argument failed.')
endif

py3 = import('python3').find_python()

ret = run_command(py3, '-c', 'print("some output")', check: false)
assert(ret.returncode() == 0, 'failed to run python3: ' + ret.stderr())
assert(ret.stdout() == 'some output\n', 'failed to run python3')

ret = run_command(py3, '-c', 'print("some output")', check: false, capture: false)
assert(ret.returncode() == 0, 'failed to run python3: ' + ret.stderr())
assert(ret.stdout() == '', 'stdout is "@0@" instead of empty'.format(ret.stdout()))

c_env = environment()
c_env.append('CUSTOM_ENV_VAR', 'FOOBAR')
ret = run_command(py3, '-c', 'import os; print(os.environ.get("CUSTOM_ENV_VAR"))', env: c_env, check: false)
assert(ret.returncode() == 0, 'failed to run python3: ' + ret.stderr())
assert(ret.stdout() == 'FOOBAR\n', 'stdout is "@0@" instead of FOOBAR'.format(ret.stdout()))

dd = find_program('dd', required : false)
if dd.found()
  ret = run_command(dd, 'if=/dev/urandom', 'bs=10', 'count=1', check: false, capture: false)
  assert(ret.returncode() == 0, 'failed to run dd: ' + ret.stderr())
  assert(ret.stdout() == '', 'stdout is "@0@" instead of empty'.format(ret.stdout()))
endif

env = environment()
env.append('MY_PATH', '1')
env.append('MY_PATH', '2')
env.prepend('MY_PATH', '0')
run_command('check-env.py', env: env, check: true)
