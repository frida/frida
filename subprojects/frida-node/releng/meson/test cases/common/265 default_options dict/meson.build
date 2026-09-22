project('test default options', 'c',
    default_options: {
        'bool': true,
        'int': 42,
        'str': 'foo',
        'array': ['foo'],
        'werror': true,
    },
)

assert(get_option('bool') == true)
assert(get_option('int') == 42)
assert(get_option('str') == 'foo')
assert(get_option('array') == ['foo'])
assert(get_option('werror') == true)

cc = meson.get_compiler('c')

# MSVC does not support #warning
if cc.get_id() != 'msvc'
  static_library('foo', 'lib.c', override_options: {'werror': false})
endif
