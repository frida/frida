project('deprecated options',
  default_options: [
    'o1=false',
    'o2=a,b',
    'o3=a,b',
    'o4=true',
    'o5=auto',
    'o6=false',
    'o8=/foo',
  ]
)

assert(get_option('o1') == false)
assert(get_option('o2') == ['a', 'b'])
assert(get_option('o3') == ['c', 'b'])
assert(get_option('o4').enabled())
assert(get_option('o5') == false)
assert(get_option('o6') == false)
assert(get_option('o7').disabled())
assert(get_option('python.platlibdir') == '/foo')
