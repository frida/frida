# Based on 'fortran/5 static', but:
#   - Uses a subproject dependency
#   - Is an install:true static library to trigger certain codepath (promotion to link_whole)
#   - Does fortran code 'generation' with configure_file
#   - Uses .F90 ext (capital F typically denotes a dependence on preprocessor treatment, which however is not used)
project('try-static-subproject-dependency', 'fortran')

static_dep = dependency('static_hello', fallback: ['static_hello', 'static_hello_dep'])

mainsrc = 'main_lib.f90'
mainsrc = configure_file(
    copy: true,
    input: mainsrc,
    output: 'main_lib_output.F90'
)
main_lib = library('mainstatic', mainsrc, dependencies: static_dep, install: true)
main_dep = declare_dependency(link_with: main_lib)

main_exe = executable('main_exe', 'main.f90', dependencies: main_dep)
test('static_subproject_test', main_exe)
