gprog = find_program('tooldir/genprog.py')

gen = generator(gprog, \
  output    : ['@BASENAME@.c', '@BASENAME@.h'],
  arguments : ['--searchdir=@CURRENT_SOURCE_DIR@', '--outdir=@BUILD_DIR@', '@INPUT@'])

generated = gen.process('subbie.inp')

e = executable('testprog', 'testprog.c', generated)
test('testprog', e)
