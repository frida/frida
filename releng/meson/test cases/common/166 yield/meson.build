project('yield_options')

subproject('sub')

assert(get_option('unshared_option') == 'one', 'Unshared option has wrong value in superproject.')
assert(get_option('shared_option') == 'two', 'Shared option has wrong value in superproject..')
assert(get_option('wrongtype_option') == 'three', 'Wrongtype option has wrong value in superproject..')
