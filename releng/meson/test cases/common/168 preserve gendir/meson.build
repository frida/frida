project('preserve subdir', 'c')

gprog = find_program('genprog.py')

gen = generator(gprog, \
  output    : ['@BASENAME@.c', '@BASENAME@.h'],
  arguments : ['--searchdir=@CURRENT_SOURCE_DIR@', '--outdir=@BUILD_DIR@', '@INPUT@'])

generated = gen.process('base.inp', 'com/mesonbuild/subbie.inp',
  preserve_path_from : meson.current_source_dir())

testcase expect_error('generator.process: When using preserve_path_from, all input files must be in a subdirectory of the given dir.')
  generated = gen.process('base.inp', 'com/mesonbuild/subbie.inp',
    preserve_path_from : meson.current_source_dir() / 'com')
endtestcase

e = executable('testprog', 'testprog.c', generated)
test('testprog', e)
