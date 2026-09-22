cc = meson.get_compiler('c')
cc_id = cc.get_id()

# PGI compiler only supports PCH for C++
if cc_id == 'pgi'
  subdir_done()
endif

exe = executable(
  'prog',
  files('main.cc', 'func.c'),
  c_pch : ['pch/func.h'],
  cpp_pch : ['pch/main.h'],
)
