project('ternary operator')

x = true
one = true ? 1 : error('False branch should not be evaluated')
two = false ? error('True branch should not be evaluated.') : 2
three = '@0@'.format(x ? 'yes' : 'no')
four = [x ? '0' : '1']

assert(one == 1, 'Return value from ternary true is wrong.')
assert(two == 2, 'Return value from ternary false is wrong.')
assert(three == 'yes', 'Return value for ternary inside method call is wrong.')
assert(four == ['0'], 'Return value for ternary inside of list is wrong.')
