project('custom target')

python3 = import('python3').find_python()

# Note that this will not add a dependency to the compiler executable.
# Code will not be rebuilt if it changes.
comp = '@0@/@1@'.format(meson.current_source_dir(), 'my_compiler.py')

mytarget = custom_target('bindat',
  output : 'data.dat',
  input : 'data_source.txt',
  capture : true,
  command : [python3, comp, '@INPUT@'],
  install : true,
  install_dir : 'subdir'
)

ct_output_exists = '''import os, sys
if not os.path.exists(sys.argv[1]):
  print("could not find {!r} in {!r}".format(sys.argv[1], os.getcwd()))
  sys.exit(1)
'''

test('capture-wrote', python3, args : ['-c', ct_output_exists, mytarget])
