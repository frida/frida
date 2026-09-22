project('endian check', 'c')

if host_machine.endian() == 'big'
  add_global_arguments('-DIS_BE', language : 'c')
endif

test('endiantest', executable('prog', 'prog.c'))
