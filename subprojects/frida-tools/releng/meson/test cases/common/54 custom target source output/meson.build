project('source generation', 'c')

ct = custom_target('gen',
output : ['mylib.h', 'mylib.c'],
command : [find_program('generator.py'), '@OUTDIR@'],
)

e = executable('prog', 'main.c', ct)
test('gentest', e)
