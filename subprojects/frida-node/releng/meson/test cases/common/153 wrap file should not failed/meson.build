project('mainproj', 'c',
  default_options : ['wrap_mode=nodownload'],
)

if not find_program('patch', required : false).found() and not find_program('git', required : false).found()
  error('MESON_SKIP_TEST: patch/git not found.')
endif

subproject('zlib')
foo = subproject('foo')
bar = subproject('bar')

libfoo = foo.get_variable('libfoo')
libbar = bar.get_variable('libbar')

executable('grabprog', files('src/subprojects/prog.c'))
executable('grabprog2', files('src/subprojects/foo/prog2.c'))
subdir('src')

subproject('patchdir')

exe = subproject('patchfile').get_variable('foo_exe')
test('test_foo', exe)
