get_prgname_sources = ['get-prgname.c', 'get-prgname.h']
get_prgname_lib = shared_library(
  'get-prgname',
  sources : get_prgname_sources,
  dependencies : [glib],
  install : false,
)

get_prgname_includes = include_directories('.')
get_prgname = declare_dependency(
  include_directories : get_prgname_includes,
  link_with : get_prgname_lib,
)
