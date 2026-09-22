configure_file(input : 'someinput.in',
  output : 'outputsubdir',
  install : false,
  copy: true)

py3 = import('python3').find_python()

cfile1 = configure_file(input : 'foo.txt',
                        output : 'foo.h.in',
                        capture : true,
                        command : [py3, '-c', 'print("#mesondefine FOO_BAR")'])
