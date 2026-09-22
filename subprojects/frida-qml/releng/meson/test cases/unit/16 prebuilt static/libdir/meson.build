cc = meson.get_compiler('c')
stlib = cc.find_library('best', dirs : meson.current_source_dir())

best_dep = declare_dependency(dependencies : stlib,
  include_directories : include_directories('.'))
