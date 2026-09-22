compiler = find_program('gen.py')
gen = generator(compiler,
  output: '@BASENAME@.h',
  arguments : ['@INPUT@', '@OUTPUT@'])
hs = gen.process(cfile, files('source'))

executable('proggie', 'main.c', hs)
