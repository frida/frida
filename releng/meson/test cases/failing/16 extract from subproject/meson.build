project('extract subproject object', 'c')

sub = subproject('sub_project')
lib = sub.get_variable('lib')

exe = executable('exe', 'main.c',
  objects : lib.extract_objects('sub_lib.c'))

test('extraction test', exe)
