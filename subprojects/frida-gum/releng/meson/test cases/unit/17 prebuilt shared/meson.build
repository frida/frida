project('prebuilt shared library', 'c')

if get_option('method') == 'find_library'
  search_dir = get_option('search_dir')
  if search_dir == 'auto'
    search_dir = meson.current_source_dir()
  endif

  cc = meson.get_compiler('c')
  shlib = cc.find_library('alexandria', dirs : search_dir)
else
  shlib = dependency('alexandria', method: get_option('method'))
endif

exe = executable('patron', 'patron.c', dependencies : shlib)
test('visitation', exe)

d = declare_dependency(dependencies : shlib)

exe2 = executable('another_visitor', 'another_visitor.c',
  dependencies : d)
test('another', exe2)

stlib = static_library(
  'rejected',
  'rejected.c',
  dependencies : shlib,
)

rejected = executable(
  'rejected',
  'rejected_main.c',
  link_with : stlib,
)
test('rejected', rejected)

rejected_whole = executable(
  'rejected_whole',
  'rejected_main.c',
  link_whole : stlib,
)
test('rejected (whole archive)', rejected_whole)
