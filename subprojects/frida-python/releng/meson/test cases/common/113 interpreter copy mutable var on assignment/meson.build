project('foo')

a = configuration_data()
a.set('HELLO', 1)

b = a

assert(a.has('HELLO'), 'Original config data should be set on a')
assert(b.has('HELLO'), 'Original config data should be set on copy')

configure_file(output : 'b.h', configuration : b)

# This should still work, as we didn't use the original above but a copy!
a.set('WORLD', 1)

assert(a.has('WORLD'), 'New config data should have been set')
assert(not b.has('WORLD'), 'New config data set should not affect var copied earlier')

configure_file(output : 'a.h', configuration : a)
