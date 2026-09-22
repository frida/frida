project('C# external library', 'cs')
glib_sharp_2 = dependency('glib-sharp-2.0', required : false)

if not glib_sharp_2.found()
  error('MESON_SKIP_TEST glib# not found.')
endif

e = executable('prog', 'prog.cs', dependencies: glib_sharp_2, install : true)
test('libtest', e, args: [join_paths(meson.current_source_dir(), 'hello.txt')])
