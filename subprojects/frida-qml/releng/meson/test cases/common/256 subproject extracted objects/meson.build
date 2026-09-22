project('link to extracted objects', 'c')

sublib = subproject('myobjects').get_variable('sublib')

mainlib = static_library('foo', 'foo.c', install: true, link_with: sublib)
