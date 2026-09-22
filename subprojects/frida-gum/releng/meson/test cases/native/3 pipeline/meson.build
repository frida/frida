project('pipeline test', 'c')

# We need to run this executable locally so build it with
# the host compiler.
e1 = executable('srcgen', 'srcgen.c', native : true)

# Generate a source file that needs to be included in the build.
gen = generator(e1, \
  depfile : '@BASENAME@.d',
  output  : '@BASENAME@.c', # Line continuation inside arguments should work without needing a "\".
  arguments : ['@INPUT@', '@OUTPUT@', '@DEPFILE@'])

generated = gen.process(['input_src.dat'])

e2 = executable('prog', 'prog.c', generated)

test('pipelined', e2)

# This is in a subdirectory to make sure
# we write proper subdir paths to output.
subdir('src')

subdir('depends')
