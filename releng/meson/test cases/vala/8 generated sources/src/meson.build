config = configure_file(input: 'config.vala.in',
  output: 'config.vala',
  configuration: cd)

print = find_program('write_wrapper.py')
wrapper = custom_target('wrapper',
  output : 'print_wrapper.vala',
  command : [print, '@OUTPUT@'])

copy = find_program('copy_file.py')
gen = generator(copy,
  output : '@BASENAME@.vala',
  arguments : ['@INPUT@', '@OUTPUT@'])

returncode = gen.process('returncode.in')

src = files('test.vala')
