project('cross find program', 'c')

native_exe = find_program('sometool.py', native : true)
cross_exe = find_program('sometool.py')
cross_other_exe = find_program('someothertool.py')

native_out = run_command(native_exe, check: true).stdout().strip()
cross_out = run_command(cross_exe, check: true).stdout().strip()
cross_other_out = run_command(cross_other_exe, check: true).stdout().strip()

assert(native_out == 'native',
  'Native output incorrect:' + native_out)
assert(cross_out == 'cross',
  'Cross output incorrect:' + cross_out)
assert(cross_out == cross_other_out,
  'Cross output incorrect:' + cross_other_out)
