project('simplejava', 'java')

one = jar('one', 'com/mesonbuild/Simple.java',
  main_class : 'com.mesonbuild.Simple',
  install : true,
  install_dir : get_option('bindir'),
)

two = jar('two', 'com/mesonbuild/Simple.java',
  main_class : 'com.mesonbuild.Simple',
  install : true,
  install_dir : get_option('bindir'),
  link_with : one,
)
