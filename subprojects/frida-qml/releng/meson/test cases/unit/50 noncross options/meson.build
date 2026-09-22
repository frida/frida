project('std_remains', 'c', default_options: ['c_std=c99'])

executable('prog', 'prog.c')

# Check that native: true does not affect the use of c_std in
# non-cross builds

if not meson.is_cross_build()
  executable('prog2', 'prog.c', native: true)

  # Check that even deps marked as native are found
  # by default when not cross compiling.
  dependency('ylib', method: 'pkg-config')
endif
