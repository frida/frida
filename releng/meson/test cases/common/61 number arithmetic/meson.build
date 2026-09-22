project('number arithmetic')

if 6 + 4 != 10
  error('Number addition is broken')
endif
if 6 - 4 != 2
  error('Number subtraction is broken')
endif

if 6 * 4 != 24
  error('Number multiplication is broken')
endif
if 16 / 4 != 4
  error('Number division is broken')
endif

#if (1 / 3) * 3 != 1
#  error('Float interconversion broken')
#endif
if (5 / 3) * 3 != 3
  error('Integer division is broken')
endif

assert((5 % 2) == 1, 'Integer modulo (odd) is broken')
assert((4 % 2) == 0, 'Integer modulo (even) is broken')

if 2 * 1 % 2 != 0
  error('Modulo precedence with multiplication is broken')
endif
if 2 + 1 % 2 != 3
  error('Modulo precedence with addition is broken')
endif
if 9 / 9 % 2 != 1
  error('Modulo precedence with division is broken')
endif
if 9 - 9 % 2 != 8
  error('Modulo precedence with subtraction is broken')
endif

assert(2.is_even(), 'int is_even() broken for even value')
assert(not(2.is_odd()), 'int is_odd() broken for even value')
assert(not(3.is_even()), 'int is_even() broken for odd value')
assert(3.is_odd(), 'int is_odd() broken for odd value')

assert(3 < 4, 'Lt broken')
assert(not(4 < 3), 'Lt broken')
assert(3 <= 4, 'Lte broken')
assert(not(4 <= 3), 'Lte broken')
assert(3 <= 3, 'Lte broken')

assert(4 > 3, 'Gt broken')
assert(not(3 > 4), 'Gt broken')
assert(4 >= 3, 'Gte broken')
assert(not(3 >= 4), 'Gte broken')
assert(3 >= 3, 'Gte broken')

assert(true.to_int() == 1,'bool to_int() broken')
assert(false.to_int() == 0,'bool to_int() broken')

hex_255 = 0xff
hex2_255 = 0XFF

assert(hex_255 == 255, 'Hex parsing is broken.')
assert(hex2_255 == 255, 'Uppercase hex parsing is broken.')

bin_123 = 0b1111011
bin2_123 = 0B1111011

assert(bin_123 == 123, 'Bin number parsing is broken.')
assert(bin2_123 == 123, 'Uppercase bin number parsing is broken.')

oct_493 = 0o755
oct2_493 = 0O755

assert(oct_493 == 493, 'Oct number parsing is broken.')
assert(oct2_493 == 493, 'Uppercase oct number parsing is broken.')
