project('project argument after target failing', 'c',
  version : '2.3.4',
  license : 'mylicense')

add_project_arguments('-DPROJECT_OPTION', language: 'c')
e = executable('exe', 'exe.c')
add_project_arguments('-DPROJECT_OPTION1', language: 'c')
