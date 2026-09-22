project('line continuation')

a = 1
b = 2

c = a \
+b
assert(c == 3, 'Line continuation is not working')

d = a + \
  b
assert(d == 3, 'Line continuation is not working')

if a == 1 and \
   b == 3
  error('Line continuation in "if" condition is not working')
endif
