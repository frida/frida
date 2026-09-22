project('custom target object output', 'c')

comp = find_program('obj_generator.py')

if host_machine.system() == 'windows'
  outputname = '@BASENAME@.obj'
else
  outputname = '@BASENAME@.o'
endif

cc = meson.get_compiler('c').cmd_array().get(-1)

subdir('objdir')
subdir('progdir')

test('objgen', e)
