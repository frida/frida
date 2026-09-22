s = custom_target(
  'subdir_target',
  command : [gen, '@OUTPUT@'],
  output : ['main.rs'],
)

l = custom_target(
  'lib_target',
  command : [gen, '@OUTPUT@', '--mode', 'lib'],
  output : ['lib.rs'],
)

lib = static_library('static_lib_generated', l)
