project('test', 'c')

generated_c = custom_target(
  'generated.c',
  input : files('generated_source.c', 'copyfile.py'),
  output : 'generated.c',
  command : ['python', '@INPUT1@', '@INPUT0@', '@OUTPUT@'],
)

static_lib = static_library(
    'static_lib',
    [files('static_lib_source.c'), generated_c],
)

both_lib = both_libraries(
    'both_lib',
    [files('both_lib_source.c')],
    link_with : [static_lib],
    install : true,
)
