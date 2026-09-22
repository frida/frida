

mytarget = custom_target('depfile',
  output : 'dep.dat',
  depfile : 'dep.dat.d',
  command : [find_program('dep.py'), meson.current_source_dir(), '@DEPFILE@', '@OUTPUT@'],
)
