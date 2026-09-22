project('install test', 'c', default_options : ['libdir=libtest'])

stlib = static_library('stat', 'stat.c', install : true)
exe = executable('prog', 'prog.c', install : true)

dirtarget = custom_target('dirtarget',
  output: ['dir'],
  install: true,
  command: [find_program('gendir.py'), '@OUTPUT@'],
  install_dir: get_option('datadir'))
