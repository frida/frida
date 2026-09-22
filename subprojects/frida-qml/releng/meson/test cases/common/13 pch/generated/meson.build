cc = meson.get_compiler('c')
cc_id = cc.get_id()

if cc_id == 'lcc'
  error('MESON_SKIP_TEST: Elbrus compiler does not support PCH.')
endif

# PGI compiler only supports PCH for C++
if cc_id == 'pgi'
  subdir_done()
endif

generated_customTarget = custom_target('makeheader',
  output: 'generated_customTarget.h',
  command : [find_program('gen_custom.py'), '@OUTPUT0@'])

generated_generator = generator(find_program('gen_generator.py'),
  output: '@BASENAME@.h',
  arguments: ['@INPUT@', '@OUTPUT@'])

exe = executable('prog', 'prog.c', generated_customTarget, generated_generator.process('generated_generator.in'),
  c_pch: 'pch/prog.h')
