project('toplevel bar', 'c')

subproject('foo')

true_cmd = find_program('true.py')

executable('bar', 'bar.c')
run_target('nop', command : [true_cmd])
custom_target('cus', output: ['cus.c'], command : [true_cmd])
