objcdummyclass = custom_target('objcdummyclass',
  input: static_library('objcdummyclass-raw', 'dummyclass.m'),
  output: 'libobjcdummyclass.a',
  command: [
    find_program('strip'),
    '-x',
    '@INPUT@',
    '-o',
    '@OUTPUT@',
  ],
)
