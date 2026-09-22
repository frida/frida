cc = meson.get_compiler('c')
cc_id = cc.get_id()

# User supplied PCH implementation should override the auto
# generated one. PCH implementations are only supported for
# msvc and generally should not be used at all. Support for
# them is only kept for backwards compatibility.
if cc_id == 'msvc'
  exe = executable('prog', 'prog.c', c_pch : ['pch/pch.h', 'pch/pch.c'])
endif
