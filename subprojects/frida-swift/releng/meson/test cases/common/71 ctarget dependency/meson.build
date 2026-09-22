project('custom target dependency')

# Sometimes custom targets do not take input files
# but instead do globbing or some similar wackiness.
# In this case we need to be able to specify a
# manual dependency between two custom targets,
# if one needs to be run before the other.

g1 = find_program('gen1.py')
g2 = find_program('gen2.py')

c1 = custom_target('medput',
input : 'input.dat',
output : 'medput.tmp',
command : [g1, '@INPUT@', '@OUTPUT@'])

custom_target('output',
output : 'output.dat',
command : [g2, '@OUTDIR@', '@OUTPUT@'],
depends : c1)
