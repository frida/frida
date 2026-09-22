project('test project')

case = get_option('case')

if case == 'find_program'
  prog = find_program('bash')
  result = run_command(prog, ['--version'], check: true)
  assert(result.stdout().strip().endswith('12345'), 'Didn\'t load bash from config file')
elif case == 'config_dep'
  add_languages('cpp')
  dep = dependency('llvm', method : 'config-tool')
  assert(dep.get_configtool_variable('version').endswith('12345'), 'Didn\'t load llvm from config file')
elif case == 'python3'
  prog = import('python3').find_python()
  result = run_command(prog, ['--version'], check: true)
  assert(result.stdout().strip().endswith('12345'), 'Didn\'t load python3 from config file')
elif case == 'python'
  prog = import('python').find_installation()
  result = run_command(prog, ['--version'], check: true)
  assert(result.stdout().strip().endswith('12345'), 'Didn\'t load python from config file')
endif
