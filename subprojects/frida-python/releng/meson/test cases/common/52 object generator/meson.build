project('object generator', 'c')

python = find_program('python3', required : false)
if not python.found()
  python = find_program('python')
endif

# Note that this will not add a dependency to the compiler executable.
# Code will not be rebuilt if it changes.
comp = '@0@/@1@'.format(meson.current_source_dir(), 'obj_generator.py')

if host_machine.system() == 'windows'
  outputname = '@BASENAME@.obj'
else
  outputname = '@BASENAME@.o'
endif

cc = meson.get_compiler('c').cmd_array().get(-1)
# Generate an object file manually.
gen = generator(python,
 output : outputname,
 arguments : [comp, cc, '@INPUT@', '@OUTPUT@'])

generated = gen.process(['source.c', 'source2.c'])

# Generate an object file with indexed OUTPUT replacement.
gen2 = generator(python,
 output : outputname,
 arguments : [comp, cc, '@INPUT@', '@OUTPUT0@'])
generated2 = gen2.process(['source3.c'])

# Generate an object file ending with .o even on Windows.
# The VS backend needs to handle .o objects differently from .obj objects.
gen3 = generator(python,
 output : '@BASENAME@.o',
 arguments : [comp, cc, '@INPUT@', '@OUTPUT@'])

generated3 = gen3.process(['source4.c'])

e = executable('prog', 'prog.c', generated, generated2, generated3)

test('objgen', e)