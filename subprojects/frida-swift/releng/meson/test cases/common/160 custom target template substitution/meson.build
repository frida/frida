project('custom target template substitution', 'c')

check = find_program('checkcopy.py')

config = configuration_data()

config_file = configure_file(configuration : config, output : 'x@IN')

# Check that substitution does not find @FOO@ and then misses @INPUT0@.
# Check the resulting x@INPUT1@ is not replaced.
foo = custom_target('runcheck',
  input : [config_file, 'foo.c.in'],
  output : 'foo.c',
  command : [check, '-D@FOO@INPUT0@PUT1@', '@INPUT1@', '@OUTPUT@']
)

executable('foo', foo)
