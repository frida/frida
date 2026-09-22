project('comparer', 'c')

# Added manually as a c_arg to test handling of include paths with backslashes
# and spaces. This is especially useful on Windows in vcxproj files since it
# stores include directories in a separate element that has its own
# context-specific escaping/quoting.
include_dir = meson.current_source_dir() + '/include'
default_c_args = ['-I' + include_dir]

if meson.get_compiler('c').get_argument_syntax() == 'msvc'
  default_c_args += ['/Faasm output\\']
  # Hack to create the 'asm output' directory in the builddir
  subdir('asm output')
endif

# Path can contain \. Here we're sending `"foo\bar"`.
test('backslash quoting',
     executable('comparer', 'comparer.c',
                c_args : default_c_args + ['-DDEF_WITH_BACKSLASH="foo\\bar"']))
# Path can end in \ without any special quoting. Here we send `"foo\bar\"`.
test('backslash end quoting',
     executable('comparer-end', 'comparer-end.c',
                c_args : default_c_args + ['-DDEF_WITH_BACKSLASH="foo\\bar\\"']))
# Path can (really) end in \ if we're not passing a string literal without any
# special quoting. Here we're sending `foo\bar\`.
test('backslash end quoting when not a string literal',
     executable('comparer-end-notstring', 'comparer-end-notstring.c',
                c_args : default_c_args + ['-DDEF_WITH_BACKSLASH=foo\\bar\\']))
