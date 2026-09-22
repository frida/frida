project('multiple dir configure file')

subdir('subdir')

configure_file(input : 'subdir/someinput.in',
  output : 'outputhere',
  copy: true)

configure_file(input : cfile1,
               output : '@BASENAME@',
               copy: true)
