project('options', 'c')

# Test passing c_args and c_link_args options from the command line.
assert(get_option('c_args') == ['-funroll-loops'],
       'Incorrect value for c_args option.')
assert(get_option('c_link_args') == ['-Dtest_harmless_but_useless_link_arg'],
       'Incorrect value for c_link_args option.')
