fake_gthread_sources = ['fake-gthread.c', 'fake-gthread.h']
fake_gthread_lib = shared_library(
  'gthread-2.0',
  sources : fake_gthread_sources,
  install : false,
)

fake_gthread_includes = include_directories('.')
fake_gthread = declare_dependency(
  include_directories : fake_gthread_includes,
  link_with : fake_gthread_lib,
)
