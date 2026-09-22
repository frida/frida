project('a', ['c'])

x = find_program('x.py')
outs = custom_target('foo', output: ['x.c', 'y'], input: 'a', command: [x])
executable('testprog', outs[0])
