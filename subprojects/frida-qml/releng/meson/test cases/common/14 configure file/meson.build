project('configure file test', 'c', meson_version: '>=0.63.0')

fs = import('fs')

conf = configuration_data()

conf.set('var', 'mystring')
conf.set('other', 'string 2')
conf.set('second', ' bonus')
conf.set('BE_TRUE', true)

assert(conf.get('var') == 'mystring', 'Get function is not working.')
assert(conf.get('var', 'default') == 'mystring', 'Get function is not working.')
assert(conf.get('notthere', 'default') == 'default', 'Default value getting is not working.')
assert(conf.keys() == ['BE_TRUE', 'other', 'second', 'var'], 'Keys function is not working')

cfile = configure_file(input : 'config.h.in',
  output : 'config.h',
  configuration : conf)

e = executable('inctest', 'prog.c',
# Note that you should NOT do this. Don't add generated headers here
# This tests that we do the right thing even if people add in conf files
# to their sources.
  cfile)
test('inctest', e)

# Test if we can also pass files() as input
configure_file(input : files('config.h.in'),
  output : 'config2.h',
  configuration : conf)

# Now generate a header file with an external script.
genprog = import('python3').find_python()
scriptfile = '@0@/generator.py'.format(meson.current_source_dir())
ifile = '@0@/dummy.dat'.format(meson.current_source_dir())
ofile = '@0@/config2.h'.format(meson.current_build_dir())

check_file = find_program('check_file.py')
# Configure in source root with command and absolute paths
outf = configure_file(input : 'dummy.dat',
  output : 'config2.h',
  command : [genprog, scriptfile,  ifile, ofile],
  install_dir : 'share/appdir')
ret = run_command(check_file, outf, check: false)
if ret.returncode() != 0
  error('Error running command: @0@\n@1@'.format(ret.stdout(), ret.stderr()))
endif

# Same again as before, but an input file should not be required in
# this case where we use a command/script to generate the output file.
genscript2b = '@0@/generator-without-input-file.py'.format(meson.current_source_dir())
ofile2b = '@0@/config2b.h'.format(meson.current_build_dir())
outf = configure_file(
  output : 'config2b.h',
  command : [genprog, genscript2b, ofile2b],
  install_dir : 'share/appdir')
ret = run_command(check_file, outf, check: false)
if ret.returncode() != 0
  error('Error running command: @0@\n@1@'.format(ret.stdout(), ret.stderr()))
endif

genscript2deps = '@0@/generator-deps.py'.format(meson.current_source_dir())
ofile2deps = '@0@/config2deps.h'.format(meson.current_build_dir())
outf = configure_file(
  output : 'config2deps.h',
  depfile : 'depfile.d',
  command : [genprog, genscript2deps, ofile2deps, '@DEPFILE@'])
ret = run_command(check_file, outf, check: false)
if ret.returncode() != 0
  error('Error running command: @0@\n@1@'.format(ret.stdout(), ret.stderr()))
endif

found_script = find_program('generator.py')
# More configure_file tests in here
subdir('subdir')

test('inctest2', executable('prog2', 'prog2.c'))

# Generate a conf file without an input file.

dump = configuration_data()
dump.set_quoted('SHOULD_BE_STRING', 'string', description : 'A string')
dump.set_quoted('SHOULD_BE_STRING2', 'A "B" C')
dump.set_quoted('SHOULD_BE_STRING3', 'A "" C')
dump.set_quoted('SHOULD_BE_STRING4', 'A " C')
dump.set('SHOULD_BE_RETURN', 'return')
dump.set('SHOULD_BE_DEFINED', true)
dump.set('SHOULD_BE_UNDEFINED', false)
dump.set('SHOULD_BE_ONE', 1)
dump.set('SHOULD_BE_ZERO', 0, description : 'Absolutely zero')
dump.set('SHOULD_BE_QUOTED_ONE', '"1"')

dump.set_quoted('INTEGER_AS_STRING', '12')
if dump.get_unquoted('INTEGER_AS_STRING').to_int() == 12
  dump.set('SHOULD_BE_UNQUOTED_STRING', dump.get_unquoted('SHOULD_BE_STRING'))
endif

configure_file(output : 'config3.h',
  configuration : dump)

test('Configless.', executable('dumpprog', 'dumpprog.c'))


# Config file generation in a loop with @BASENAME@ substitution
dump = configuration_data()
dump.set('ZERO', 0)
config_templates = files(['config4a.h.in', 'config4b.h.in'])
foreach config_template : config_templates
  configure_file(input : config_template, output : '@BASENAME@', configuration : dump)
endforeach

test('Substituted', executable('prog4', 'prog4.c'))

# Test `capture` keyword

basename_py = find_program('basename.py')
file_contains_py = find_program('file_contains.py')
test_string = 'hello world'
test_input_file = join_paths(meson.current_build_dir(), test_string)
run_command(find_program('touch.py'), test_input_file, check: true)
configs = [
    # no input
    configure_file(command: [ basename_py, test_string ], capture: true, output: 'capture test 1'),
    # with input
    configure_file(input: test_input_file, command: [ basename_py, '@INPUT@' ], capture: true, output: 'capture test 2'),
]
foreach c : configs
    test('@0@'.format(c), file_contains_py, args: [ c, test_string ])
endforeach

# Test variable is substituted only once
conf5 = configuration_data()
conf5.set('var', '@var2@')
conf5.set('var2', 'error')
configure_file(
  input : 'config5.h.in',
  output : '@BASENAME@',
  configuration : conf5)
test('test5', executable('prog5', 'prog5.c'))

# Test escaping
conf6 = configuration_data()
conf6.set('var1', 'foo')
conf6.set('var2', 'bar')
configure_file(
  input : 'config6.h.in',
  output : '@BASENAME@',
  configuration : conf6)
test('test6', executable('prog6', 'prog6.c'))

# test empty install dir string
cfile = configure_file(input : 'config.h.in',
  output : 'do_not_get_installed.h',
  install_dir : '',
  configuration : conf)

# test install_dir : false (deprecated)
cfile = configure_file(input : 'config.h.in',
  output : 'do_not_get_installed_please.h',
  install_dir : false,
  configuration : conf)

# test install_dir with install: false
cfile = configure_file(input : 'config.h.in',
  output : 'do_not_get_installed_in_install_dir.h',
  install : false,
  install_dir : 'share/appdir',
  configuration : conf)

# Test escaping with cmake format
conf7 = configuration_data()
conf7.set('var1', 'foo')
conf7.set('var2', 'bar')
configure_file(
  input : 'config7.h.in',
  output : '@BASENAME@',
  format : 'cmake',
  configuration : conf7)
test('test7', executable('prog7', 'prog7.c'))

# Test copying of an empty configuration data object
inf = 'invalid-utf8.bin.in'
outf = configure_file(input : inf,
  output : 'invalid-utf8.bin',
  copy: true)
ret = run_command(check_file, inf, outf, check: false)
if ret.returncode() != 0
  error('Error running command: @0@\n@1@'.format(ret.stdout(), ret.stderr()))
endif
# Now the same, but using a File object as an argument.
inf2 = files('invalid-utf8.bin.in')[0]
outf = configure_file(input : inf2,
  output : 'invalid-utf8.bin',
  copy: true)
ret = run_command(check_file, inf2, outf, check: false)
if ret.returncode() != 0
  error('Error running command: @0@\n@1@'.format(ret.stdout(), ret.stderr()))
endif

# Test copy of a binary file
outf = configure_file(input : inf,
  output : 'somebinary.bin',
  copy : true)
ret = run_command(check_file, inf, outf, check: false)
if ret.returncode() != 0
  error('Error running command: @0@\n@1@'.format(ret.stdout(), ret.stderr()))
endif

# Test the fs replacement
# Test copying of an empty configuration data object
inf = 'invalid-utf8.bin.in'
outf = fs.copyfile(inf, 'invalid-utf8-1.bin',
  install: true,
  install_dir: get_option('datadir') / meson.project_name(),
  install_tag: 'copyfile',
)
test('fs.copyfile string', check_file, args: [files(inf), outf])

# Test with default outname of string
outf = fs.copyfile(inf)
test('fs.copyfile default name', check_file, args: [files(inf), outf])

# Now the same, but using a File object as an argument.
inf2 = files('invalid-utf8.bin.in')[0]
outf = fs.copyfile(inf2, 'invalid-utf8-2.bin')
test('fs.copyfile file', check_file, args: [inf2, outf])

# Test non isolatin1 encoded input file which fails to decode with utf-8
conf8 = configuration_data()
conf8.set('var', 'foo')
configure_file(
  input : 'config8.h.in',
  output : '@BASENAME@',
  encoding : 'koi8-r',
  configuration : conf8)

# Test that passing an empty configuration_data() object to a file with
# #mesondefine substitutions does not print the warning.
configure_file(
  input: 'nosubst-nocopy1.txt.in',
  output: 'nosubst-nocopy1.txt',
  configuration : configuration_data())

# test that passing an empty configuration_data() object to a file with
# @foo@ substitutions does not print the warning.
configure_file(
  input: 'nosubst-nocopy2.txt.in',
  output: 'nosubst-nocopy2.txt',
  configuration : configuration_data())

# test that passing a configured file object to test() works, and that passing
# an empty configuration_data() object to a file that leads to no substitutions
# prints a warning (see unit tests)
test_file = configure_file(
  input: 'test.py.in',
  output: 'test.py',
  configuration: configuration_data())

# Test that overwriting an existing file creates a warning.
configure_file(
  input: 'test.py.in',
  output: 'double_output.txt',
  configuration: conf)
configure_file(
  input: 'test.py.in',
  output: 'double_output.txt',
  configuration: conf)

# Test that the same file name in a different subdir will not create a warning
configure_file(
  input: 'test.py.in',
  output: 'no_write_conflict.txt',
  configuration: conf)

# Test that @BASENAME@ is substituted before checking and does not create a warning.
configure_file(
  input: 'differentafterbasename1.in',
  output: '@BASENAME@',
  configuration: conf
)
configure_file(
  input: 'differentafterbasename2.in',
  output: '@BASENAME@',
  configuration: conf
)

# Test that @BASENAME@ is substituted before checking and does create a warning on conflict.
configure_file(
  input: 'sameafterbasename.in',
  output: '@BASENAME@',
  configuration: conf
)
configure_file(
  input: 'sameafterbasename.in2',
  output: '@BASENAME@',
  configuration: conf
)

test('configure-file', test_file)

# Dictionaries

cdata = configuration_data({
  'A_STRING' : '"foo"',
  'A_INT' : 42,
  'A_DEFINED' : true,
  'A_UNDEFINED' : false,
})

configure_file(output : 'config9a.h',
  configuration : cdata,
)

configure_file(output : 'config9b.h',
  configuration : {
    'B_STRING' : '"foo"',
    'B_INT' : 42,
    'B_DEFINED' : true,
    'B_UNDEFINED' : false,
  }
)

test('test9', executable('prog9', 'prog9.c'))

check_inputs = find_program('check_inputs.py')
configure_file(output : 'check_inputs.txt',
  input : ['prog.c', files('prog2.c', 'prog4.c')],
  command : [check_inputs, '@OUTPUT@', '@INPUT0@', '@INPUT@', files('prog5.c')]
)
