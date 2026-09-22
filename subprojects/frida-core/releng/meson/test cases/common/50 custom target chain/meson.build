project('custom target', 'c')

python = find_program('python3', required : false)
if not python.found()
  python = find_program('python')
endif

# files() is the correct way to do this, but some people
# do this so test that it works.
comp = '@0@/@1@'.format(meson.current_source_dir(), 'my_compiler.py')
comp2 = '@0@/@1@'.format(meson.current_source_dir(), 'my_compiler2.py')
infile = files('data_source.txt')[0]

mytarget = custom_target('bindat',
  output : 'data.dat',
  command : [python, comp, infile, '@OUTPUT@'],
)

mytarget2 = custom_target('bindat2',
  output : 'data2.dat',
  command : [python, comp2, mytarget, '@OUTPUT@'],
  install : true,
  install_dir : 'subdir'
)

mytarget3 = custom_target('bindat3',
  output : 'data3.dat',
  input : [mytarget],
  command : [python, comp2, '@INPUT@', '@OUTPUT@'],
  install : true,
  install_dir : 'subdir'
)

subdir('usetarget')
