project('run target', 'c')

# Make it possible to run built programs.
# In cross builds exe_wrapper should be added if it exists.

exe = executable('helloprinter', 'helloprinter.c')

if not meson.is_cross_build() or meson.can_run_host_binaries()
  run_target('runhello',
    command : [exe, 'argument'])
endif

converter = find_program('converter.py')

hex = custom_target('exe.hex',
  input : exe,
  output : 'exe.hex',
  command : [converter, '@INPUT@', '@OUTPUT@',
  ],
)

fakeburner = find_program('fakeburner.py')

# These emulates the Arduino flasher application. It sandwiches the filename inside
# a packed argument. Thus we need to declare it manually.
run_target('upload',
  command : [fakeburner, 'x:@0@:y'.format(exe.full_path())],
  depends : exe,
)

run_target('upload2',
  command : [fakeburner, 'x:@0@:y'.format(hex.full_path())],
  depends : hex,
)

python3 = find_program('python3', required : false)
if not python3.found()
  python3 = find_program('python')
endif

run_target('py3hi',
  command : [python3, '-c', 'print("I am Python3.")'])

run_target('check_exists',
  command : [find_program('check_exists.py'), files('helloprinter.c')])

run_target('check_exists',
  command : [find_program('check_exists.py'), files('helloprinter.c')],
  depends : disabler(),
)

run_target('check_exists',
  command : [disabler(), files('helloprinter.c')])

# What if the output of a custom_target is the command to
# execute. Obviously this will not work as hex is not an
# executable but test that the output is generated correctly.
run_target('donotrunme',
  command : hex)

# Ensure configure files can be passed
conf = configure_file(
  input: 'configure.in',
  output: 'configure',
  configuration: configuration_data()
)

run_target('configure_script',
  command : conf
)

custom_target('configure_script_ct',
  command: conf,
  output: 'dummy.txt',
  capture: true)

# Target names that clash with potential builtin functionality.
run_target('ctags',
  command : converter)

clangf = run_target('clang-format',
  command : [converter, files('.clang-format'), meson.current_build_dir() / 'clang-format'])
custom_target('clang-tidy',
  input: '.clang-tidy',
  output: 'clang-tidy',
  command : [converter, '@INPUT@', '@OUTPUT@'])
alias_target('clang-format-check', clangf)

# Check we can pass env to the program. Also check some string substitutions
# that were added in 0.57.0 but not documented. This is documented behaviour
# since 0.57.1.
run_target('check-env',
  command: [find_program('check-env.py'), '@SOURCE_ROOT@', '@BUILD_ROOT@',
            '@CURRENT_SOURCE_DIR@'],
  env: {'MY_ENV': '1'},
)

# Check some string substitutions that has always been done but never documented.
# Some projects have been relying on this implementation detail. This is
# documented behaviour since 0.57.1.
custom_target('check-env-ct',
  command: [find_program('check-env.py'), '@SOURCE_ROOT@', '@BUILD_ROOT@',
            '@CURRENT_SOURCE_DIR@'],
  env: {'MESON_SOURCE_ROOT': meson.source_root(),
        'MESON_BUILD_ROOT': meson.build_root(),
        'MESON_SUBDIR': meson.current_source_dir(),
        'MESONINTROSPECT': 'fake value',
        'MY_ENV': '1'},
  output: 'check-env-ct',
)

run_target('textprinter', command: ['subdir/textprinter.py'])
