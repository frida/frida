project('disabler_inside_array', 'c')

exes = []

exes += library('a', 'test.c')

exes += library('b', 'test.c', dependencies : disabler())

exes += library('c', 'test.c')
