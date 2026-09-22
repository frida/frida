project('selfdep', 'fortran')

e = executable('selfdep', 'selfdep.f90')
test('selfdep', e)

library('selfmod', 'src/selfdep_mod.f90')

subproject('sub1')
