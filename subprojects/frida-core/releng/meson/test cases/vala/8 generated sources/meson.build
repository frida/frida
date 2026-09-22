project('mytest', 'vala', 'c')

cd = configuration_data()
cd.set('x', 'y')

subdir('src')

executable('generatedtestparent', [src, config, returncode, wrapper],
  install : true,
  dependencies: [dependency('glib-2.0'), dependency('gobject-2.0')])

subdir('tools')
subdir('onlygen')
subdir('dependency-generated')
