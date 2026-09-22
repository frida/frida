project('test', 'c')

static_library('lib', 'lib.c')
inc = include_directories('inc')
add_project_dependencies(declare_dependency(include_directories: inc), language: 'c')
