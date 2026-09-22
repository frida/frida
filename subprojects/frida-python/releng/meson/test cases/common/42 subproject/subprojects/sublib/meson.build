project('subproject', 'c',
  version : '1.0.0',
  license : ['sublicense1', 'sublicense2'],
  license_files: ['sublicense1.txt', 'sublicense2.txt'],
)

if not meson.is_subproject()
  error('Claimed to be master project even though we are a subproject.')
endif

assert(meson.project_name() == 'subproject', 'Incorrect subproject name')

if meson.project_version() != '1.0.0'
  error('Incorrect version string in subproject.')
endif

i = include_directories('include')
l = shared_library('sublib', 'sublib.c', include_directories : i, install : false,
 c_args : '-DBUILDING_SUB=2')
t = executable('simpletest', 'simpletest.c', include_directories : i, link_with : l)
test('plain', t)
