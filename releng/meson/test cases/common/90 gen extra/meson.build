project('extra args in gen', 'c')

prog = find_program('srcgen.py')

gen = generator(prog,
  output : '@BASENAME@.c',
  arguments : ['--input=@INPUT@', '--output=@OUTPUT@', '@EXTRA_ARGS@'])

g1 = gen.process('name.dat')
g2 = gen.process('name.dat', extra_args: '--upper')

test('basic', executable('basic', 'plain.c', g1))
test('upper', executable('upper', 'upper.c', g2))

prog2 = find_program('srcgen2.py')
basename_gen = generator(prog2,
  output : ['@BASENAME@.tab.c', '@BASENAME@.tab.h'],
  arguments : ['@BUILD_DIR@', '@BASENAME@', '@INPUT@'])

basename_src = basename_gen.process('name.l')

test('basename', executable('basename', basename_src))

plainname_gen = generator(prog2,
  output : ['@PLAINNAME@.tab.c', '@PLAINNAME@.tab.h'],
  arguments : ['@BUILD_DIR@', '@PLAINNAME@', '@INPUT@'])

plainname_src = plainname_gen.process('name.l')

test('plainname', executable('plainname', plainname_src))

prog3 = find_program('srcgen3.py')
capture_gen = generator(prog3,
  output : ['@BASENAME@.yy.c'],
  arguments : ['@INPUT@'],
  capture : true)

capture_src = capture_gen.process('name.l')

test('capture', executable('capture', capture_src))
