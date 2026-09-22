project('plusassign')

x = []

x += 'a'

if x.length() != 1
  error('Incorrect append')
endif

if x[0] != 'a'
  error('Incorrect append 2.')
endif

y = x

x += 'b'

if y.length() != 1
  error('Immutability broken.')
endif

if y[0] != 'a'
  error('Immutability broken 2.')
endif

if x.length() != 2
  error('Incorrect append 3')
endif

if x[0] != 'a'
  error('Incorrect append 4.')
endif

if x[1] != 'b'
  error('Incorrect append 5.')
endif

# Now with evil added: append yourself.

x += x

if x.length() != 4
  error('Incorrect selfappend.')
endif

# += on strings

bra = 'bra'
foo = 'A'
foo += bra
foo += 'cada'
foo += bra
assert (foo == 'Abracadabra', 'string += failure [@0@]'.format(foo))
assert (bra == 'bra', 'string += modified right argument!')
foo += ' ' + foo
assert (foo == 'Abracadabra Abracadabra', 'string += failure [@0@]'.format(foo))

# += on ints

foo = 5
foo += 6
assert (foo == 11, 'int += failure [@0@]'.format(foo))
bar = 99
foo += bar
assert (foo == 110, 'int += failure [@0@]'.format(foo))
assert (bar == 99, 'int += modified right argument"')
bar += foo + 1
assert (bar == 210, 'int += failure [@0@]'.format(bar))
assert (foo == 110, 'int += modified right argument"')
