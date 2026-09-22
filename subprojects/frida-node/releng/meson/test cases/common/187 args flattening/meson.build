project('args flattening')

arr = get_variable('does-not-exist', ['bar', 'baz'])
assert(arr == ['bar', 'baz'], 'get_variable with array fallback is broken')

set_variable('arr', ['bar', 'baz'])
assert(arr == ['bar', 'baz'], 'set_variable(array) is broken')

arr = meson.get_cross_property('does-not-exist', ['bar', 'baz'])
assert(arr == ['bar', 'baz'], 'meson.get_cross_property with array fallback is broken')

arr = meson.get_external_property('does-not-exist', ['bar', 'baz'])
assert(arr == ['bar', 'baz'], 'meson.get_external_property with array fallback is broken')

arr = meson.get_external_property('does-not-exist', ['bar', 'baz'], native: true)
assert(arr == ['bar', 'baz'], 'meson.get_external_property native:true with array fallback is broken')

arr = meson.get_external_property('does-not-exist', ['bar', 'baz'], native: false)
assert(arr == ['bar', 'baz'], 'meson.get_external_property native:false with array fallback is broken')

# Test deprecated behaviour

conf = configuration_data()
conf.set(['foo', 'bar'])
message(conf.get('foo'))
