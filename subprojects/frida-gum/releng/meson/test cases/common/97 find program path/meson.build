project('find program')

python = import('python3').find_python()

# Source file via string
prog = find_program('program.py')
# Source file via files()
progf = files('program.py')
# Built file
py = configure_file(input : 'program.py',
                    output : 'builtprogram.py',
                    configuration : configuration_data())

foreach f : [prog, progf, py, find_program(py), find_program(progf)]
  ret = run_command(python, f, check: false)
  assert(ret.returncode() == 0, 'can\'t manually run @0@'.format(prog.path()))
  assert(ret.stdout().strip() == 'Found', 'wrong output from manually-run @0@'.format(prog.path()))

  ret = run_command(f, check: false)
  assert(ret.returncode() == 0, 'can\'t run @0@'.format(prog.path()))
  assert(ret.stdout().strip() == 'Found', 'wrong output from @0@'.format(prog.path()))
endforeach
