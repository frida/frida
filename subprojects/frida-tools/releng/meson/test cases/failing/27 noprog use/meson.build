project('using not found exe')

nope = find_program('nonexisting', required : false)

custom_target( 'aa',
  input: 'meson.build',
  output: 'foobar',
  command: [nope, '@INPUT@', '@OUTPUT@']
)
