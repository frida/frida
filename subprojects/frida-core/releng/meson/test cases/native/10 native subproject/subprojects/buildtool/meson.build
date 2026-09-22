project('buildtool', 'c', meson_version : '>= 1.3.0')

maingen = find_program('maingen')

ct = custom_target(
  'gen',
  command : [maingen],
  output : 'gen.c',
  capture : true,
)

e = executable('buildtool', 'buildtool.c', ct)

meson.override_find_program('buildtool', e)

subproject('hostp', native : false)
