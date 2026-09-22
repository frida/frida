project('global arg test', 'cpp', 'c')

add_global_arguments('-DMYTHING', language : 'c', native : true)
add_global_arguments('-DMYCPPTHING', language : 'cpp', native : true)
add_global_arguments('-DGLOBAL_BUILD', language : 'c', native : true)

build_c_args = ['-DARG_BUILD']
c_args = ['-DARG_HOST']

add_global_arguments('-DMYCANDCPPTHING', language: ['c', 'cpp'], native: true)

exe1 = executable('prog1', 'prog.c', c_args : build_c_args, native : true)

test('prog1', exe1)
