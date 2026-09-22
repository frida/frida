project('multiwrap', 'c',
  default_options : 'c_std=c99')

# Using multiple downloaded projects for great justice.

cc = meson.get_compiler('c')

luadep = dependency('lua', fallback : ['lua', 'lua_dep'])
pngdep = dependency('libpng', fallback : ['libpng', 'png_dep'])

executable('prog', 'prog.c',
  dependencies : [pngdep, luadep])
