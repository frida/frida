# The Ninja backend used to try and pass -sourcepath repeatedly for
# multiple includes which would discard prior includes. Since this
# won't compile without the '.' include, this ensures that multiple
# paths are passed in a [semi-]colon separated list instead...

project('includedirsjava', 'java')

javaprog = jar('myprog',
 'com/mesonbuild/Simple.java',
 'com/mesonbuild/TextPrinter.java',
  main_class : 'com.mesonbuild.Simple',
  include_directories : [ include_directories('com'),
                          include_directories('com/mesonbuild') ])
test('subdirtest', javaprog)
