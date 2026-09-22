project('flex and bison', 'c')

# The point of this test is that one generator
# may output headers that are necessary to build
# the sources of a different generator.

flex = find_program('flex', required: false)
bison = find_program('bison', required: false)

if not flex.found()
  error('MESON_SKIP_TEST flex not found.')
endif

if not bison.found()
  error('MESON_SKIP_TEST bison not found.')
endif

lgen = generator(flex,
output : '@PLAINNAME@.yy.c',
arguments : ['-o', '@OUTPUT@', '@INPUT@'])

lfiles = lgen.process('lexer.l')

pgen = generator(bison,
output : ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
arguments : ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

pfiles = pgen.process('parser.y')

e = executable('pgen', 'prog.c',
    lfiles,
    pfiles,
    override_options: 'unity=off')

test('parsertest', e,
    args: [meson.current_source_dir() / 'testfile'])
