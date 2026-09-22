project('subproject', 'c',
  version : '1.0.0',
  license : ['sublicense1', 'sublicense2'])

if not meson.is_subproject()
  error('Claimed to be master project even though we are a subproject.')
endif

assert(meson.project_name() == 'subproject', 'Incorrect subproject name')

add_project_arguments('-DSUBPROJECT_OPTION', language: 'c')
e = executable('subexe', 'subexe.c')
test('subexetest', e)
