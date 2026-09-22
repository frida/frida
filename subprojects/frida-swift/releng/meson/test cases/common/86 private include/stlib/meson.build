genbin = find_program('compiler.py')

gen = generator(genbin,
  output : ['@BASENAME@.h', '@BASENAME@.c'],
  arguments : ['@INPUT@', '@BUILD_DIR@']
  )

defs = ['foo1.def', 'foo2.def']
generated = gen.process(defs)

stlib = static_library('st', generated)
st_priv_inc = stlib.private_dir_include()
