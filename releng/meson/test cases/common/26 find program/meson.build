project('find program')

if build_machine.system() == 'windows'
  # Things Windows does not provide:
  #  - an executable to copy files without prompting
  #  - working command line quoting
  #  - anything that you might actually need
  # Because of these reasons we only check that
  # the program can be found.
  cp = find_program('xcopy')
else
  cp = find_program('donotfindme', 'cp')
  gen = generator(cp, \
   output  : '@BASENAME@.c', \
   arguments : ['@INPUT@', '@OUTPUT@'])

  generated = gen.process('source.in')
  add_languages('c', required: true)
  e = executable('prog', generated)
  test('external exe', e)
endif

prog = find_program('print-version.py', version : '>=2.0', required : false)
assert(not prog.found(), 'Version should be too old')

prog = find_program('print-version.py', version : '>=1.0')
assert(prog.found(), 'Program version should match')

prog = find_program('print-version.py')
assert(prog.version() == '1.0', 'Program version should be detectable')

prog = find_program('print-version-with-prefix.py', version : '>=1.0')
assert(prog.found(), 'Program version should match')

prog = find_program('test_subdir.py', required : false)
assert(not prog.found(), 'Program should not be found')

prog = find_program('test_subdir.py', dirs : ['/nonexistent', meson.current_source_dir() / 'scripts'])
assert(prog.found(), 'Program should be found')
