project('compiler_object_in_run_command', 'c')
cc = meson.get_compiler('c')

# This test only checks that the compiler object can be passed to
# run_command(). If the compiler has been launched, it is expected
# to output something either to stdout or to stderr.
result = run_command(cc, '--version', check: false)
if result.stdout() == '' and result.stderr() == ''
  error('No output in stdout and stderr. Did the compiler run at all?')
endif
