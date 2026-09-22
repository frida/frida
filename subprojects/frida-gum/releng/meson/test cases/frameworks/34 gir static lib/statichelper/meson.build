conf = configuration_data()
conf.set('HEADER', 'glib-object.h')

meson_sample_header = configure_file(
  input : 'meson-sample.h.in',
  output : 'meson-sample.h',
  configuration : conf)

statichelper_sources = files('meson-sample.c') + [meson_sample_header]

statichelper_lib = static_library(
  'statichelper',
  sources : statichelper_sources,
  dependencies : gobj,
)

statichelper_inc = include_directories('.')
statichelper_dep = declare_dependency(
  link_with: statichelper_lib,
  dependencies : gobj,
  include_directories : [statichelper_inc],
)
