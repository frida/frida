# Configure in subdir with absolute paths for input and relative for output
configure_file(input : '../dummy.dat',
  output : 'config2-1.h',
  command : [genprog, scriptfile,  ifile, 'config2-1.h'],
  install_dir : 'share/appdireh')
run_command(check_file, join_paths(meson.current_build_dir(), 'config2-1.h'), check: true)

# Configure in subdir with files() for input and relative for output
configure_file(input : '../dummy.dat',
  output : 'config2-2.h',
  command : [genprog, scriptfile, files('../dummy.dat'), 'config2-2.h'],
  install_dir : 'share/appdirok')
run_command(check_file, join_paths(meson.current_build_dir(), 'config2-2.h'), check: true)

# Configure in subdir with string templates for input and output
configure_file(input : '../dummy.dat',
  output : 'config2-3.h',
  command : [found_script,  '@INPUT@', '@OUTPUT@'])
run_command(check_file, join_paths(meson.current_build_dir(), 'config2-3.h'), check: true)

# Test that overwriting an existing file creates a warning.
configure_file(
  input: '../test.py.in',
  output: 'double_output2.txt',
  configuration: conf
)
configure_file(
  input: '../test.py.in',
  output: 'double_output2.txt',
  configuration: conf
)

# Test that the same file name in a different subdir will not create a warning
configure_file(
  input: '../test.py.in',
  output: 'no_write_conflict.txt',
  configuration: conf
)
