gen = find_program('codegen') # Should use overridden value set in "subdir".

src = custom_target('arrival',
  input : 'source.desc',
  output : 'file.c',
  command : [gen, '@INPUT@', '@OUTPUT@']
  )

e = executable('six', 'main.c', src)

test('six', e)

# Override stuff with an executables
meson.override_find_program('six_meson_exe', e)


# The same again, but this time with a program that was generated
# with configure_file.

gen = find_program('gencodegen')

src = custom_target('hundred',
  input : 'source2.desc',
  output : 'file2.c',
  command : [gen, '@INPUT@', '@OUTPUT@']
  )

e = executable('hundred', 'main2.c', src)

test('hundred', e)
