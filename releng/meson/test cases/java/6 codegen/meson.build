# If we generate code under the build directory then the backend needs to add
# the build directory to the -sourcepath passed to javac otherwise the compiler
# won't be able to handle the -implicit:class behaviour of automatically
# compiling dependency classes.

project('codegenjava', 'java')

subdir('com/mesonbuild')

javaprog = jar('myprog',
 config_file,
 'com/mesonbuild/Simple.java',
 'com/mesonbuild/TextPrinter.java',
  main_class : 'com.mesonbuild.Simple')
test('subdirtest', javaprog)
