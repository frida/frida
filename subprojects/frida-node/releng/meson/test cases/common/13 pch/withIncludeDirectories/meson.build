cc = meson.get_compiler('c')
cc_id = cc.get_id()

if cc_id == 'lcc'
  error('MESON_SKIP_TEST: Elbrus compiler does not support PCH.')
endif

# PGI compiler only supports PCH for C++
if cc_id == 'pgi'
  subdir_done()
endif

exe = executable('prog', 'prog.c',
  include_directories: 'include',
  c_pch : 'pch/prog.h')
