project('simplejava', 'java')

javaprog = jar('myprog', 'com/mesonbuild/Simple.java',
  main_class : 'com.mesonbuild.Simple',
  install : true,
  install_dir : get_option('bindir'))
test('mytest', javaprog)

jc = meson.get_compiler('java')
message(jc.get_id())
message(jc.get_linker_id())
