project('project options tester', 'c', 'cpp',
  version : '2.3.4',
  license : 'mylicense')

add_global_arguments('-DGLOBAL_ARGUMENT', language: 'c')
add_project_arguments('-DPROJECT_OPTION', language: 'c')
add_project_arguments('-DPROJECT_OPTION_CPP', language: 'cpp')
add_project_arguments('-DPROJECT_OPTION_C_CPP', language: ['c', 'cpp'])

sub = subproject('subexe', version : '1.0.0')

add_project_arguments('-DPROJECT_OPTION_1', language: 'c')

e = executable('exe', 'exe.c')
e = executable('execpp', 'exe.cpp')
test('exetest', e)
test('execpptest', e)
