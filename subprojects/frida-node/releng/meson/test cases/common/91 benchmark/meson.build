project('benchmark', 'c')

delayer = executable('delayer', 'delayer.c', c_args : '-D_GNU_SOURCE')
benchmark('delayer', delayer)
