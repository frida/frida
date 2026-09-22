project('own libc', 'c')

# Not related to this test, but could not find a better place for this test.
assert(meson.get_cross_property('nonexisting', 'defaultvalue') == 'defaultvalue',
  'Cross prop getting is broken.')

# A simple project that uses its own libc.

# Note that we don't need to specify anything, the flags to use
# stdlib come from the cross file.

exe = executable('selfcontained', 'prog.c')

test('standalone test', exe)
