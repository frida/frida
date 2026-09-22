project('subproject', 'c',
  version : '1.0.0',
  license : ['sublicense1', 'sublicense2'])

i = include_directories('include')
shared_library('sublib', 'sublib.c',
  version : '2.1.0',
  soversion : 5,
  include_directories : i, install : true,
  c_args : '-DBUILDING_SUB=2')
