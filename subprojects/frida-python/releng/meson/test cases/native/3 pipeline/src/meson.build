e1 = executable('srcgen', 'srcgen.c', native : true)

# Generate a header file that needs to be included.
gen = generator(e1,
 output  : '@BASENAME@.h',
 arguments : ['@INPUT@', '@OUTPUT@'])

generated = gen.process('input_src.dat')

e2 = executable('prog', 'prog.c', generated)

test('pipelined', e2)
