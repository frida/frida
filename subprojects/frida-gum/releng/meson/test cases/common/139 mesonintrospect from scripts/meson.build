project('mesonintrospect from scripts')

python = import('python3').find_python()

ret = run_command(python, ['check_env.py', '1'], check: false)
if ret.returncode() == 0
  find_program(ret.stdout())
else
  message(ret.stdout())
  message(ret.stderr())
endif

meson.add_postconf_script('check_introspection.py')
meson.add_install_script('check_env.py')
