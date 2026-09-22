
#mesonlib.File built
source4 = configure_file(input : 'source4.c',
  output : 'source4' + ext,
  command : [comp, cc, files('source4.c'),
             join_paths(meson.current_build_dir(), 'source4' + ext)])

subdirfilebuilt_obj = static_library('subdirfilebuilt_obj', objects : source4)


#mesonlib.File not built
configure_file(input : 'source5.c',
  output : 'source5' + ext,
  command : [comp, cc, files('source5.c'),
             join_paths(meson.current_build_dir(), 'source5' + ext)])

subdirfile_obj = static_library('subdirfile_obj', objects : files(meson.current_build_dir()/'source5' + ext))


#str
configure_file(input : 'source6.c',
  output : 'source6' + ext,
  command : [comp, cc, files('source6.c'),
             join_paths(meson.current_build_dir(), 'source6' + ext)])


subdirstr_obj = static_library('subdirstr_obj', objects : meson.current_build_dir()/'source6' + ext)
