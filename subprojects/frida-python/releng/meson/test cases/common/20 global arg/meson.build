project('global arg test', 'cpp', 'c')

add_global_arguments('-DMYTHING', language : 'c')
add_global_arguments('-DMYCPPTHING', language : 'cpp')
add_global_arguments('-DGLOBAL_HOST', language : 'c')

build_c_args = ['-DARG_BUILD']
c_args = ['-DARG_HOST']

add_global_arguments('-DMYCANDCPPTHING', language: ['c', 'cpp'])

exe2 = executable('prog2', 'prog.c', c_args : c_args)
exe3 = executable('prog3', 'prog.cc')

test('prog2', exe2)
test('prog3', exe3)
