cc = meson.get_compiler('c')
cc_id = cc.get_id()

if cc_id == 'lcc'
  error('MESON_SKIP_TEST: Elbrus compiler does not support PCH.')
endif

if cc.get_argument_syntax() == 'gcc'
  c_args = ['-include', 'locale.h']
elif cc.get_argument_syntax() == 'msvc'
  c_args = ['/FI' + 'locale.h']
else
  subdir_done()
endif

exe = executable('prog', 'prog.c',
c_args: c_args,
c_pch : 'pch/prog.h')
