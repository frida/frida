project('custom target input extracted objects', 'c')

if meson.backend() == 'xcode'
    error('MESON_SKIP_TEST: sometimes Xcode puts object files in weird paths and we cannot extract them.')
endif


checker = find_program('check_object.py')

cc = meson.get_compiler('c').cmd_array().get(-1)

subdir('libdir')

custom_target('check',
  input: objlib.extract_objects('source.c'),
  output: 'objcheck',
  command: [checker, '1', '@OUTPUT@', '@INPUT@'],
  build_by_default: true)

custom_target('checkct',
  input: objlib.extract_objects(ctsrc),
  output: 'objcheck-ct',
  command: [checker, '1', '@OUTPUT@', '@INPUT@'],
  build_by_default: true)

custom_target('checkcti',
  input: objlib.extract_objects(ctsrc[0]),
  output: 'objcheck-cti',
  command: [checker, '1', '@OUTPUT@', '@INPUT@'],
  build_by_default: true)

custom_target('checkgen',
  input: objlib.extract_objects(gensrc),
  output: 'objcheck-gen',
  command: [checker, '1', '@OUTPUT@', '@INPUT@'],
  build_by_default: true)

custom_target('checkall',
  input: objlib.extract_all_objects(recursive: false),
  output: 'objcheck-all',
  command: [checker, '3', '@OUTPUT@', '@INPUT@'],
  build_by_default: true)

custom_target('checkall-recursive',
  input: objlib.extract_all_objects(recursive: true),
  output: 'objcheck-all-recursive',
  command: [checker, '4', '@OUTPUT@', '@INPUT@'],
  build_by_default: true)
