project('pch', 'c')
# It is not allowed to have the PCH implementation in a different
# folder than the header.
exe = executable('prog', 'prog.c',
  c_pch : ['include/pch.h', 'src/pch.c'])
