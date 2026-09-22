project('get prop')

x = meson.get_external_property('astring')
ref = meson.is_cross_build() ? 'cross' : 'mystring'
assert(x==ref, 'did not get native property string. did you use "meson setup --native-file native.txt"')

x = meson.get_external_property('astring', native: true)
assert(x=='mystring', 'did not get native property with native:true and non-cross build.')

x = meson.get_external_property('astring', 'fallback', native: false)
assert(x==ref, 'did not get native property with native:false and non-cross build.')


x = meson.get_external_property('nonexistent', 'fallback')
assert(x=='fallback', 'fallback did not work')

x = meson.get_external_property('nonexistent', 'fallback', native: true)
assert(x=='fallback', 'fallback native:true did not work')

x = meson.get_external_property('nonexistent', 'fallback', native: false)
assert(x=='fallback', 'fallback native:false did not work')


x = meson.get_external_property('anarray')
assert(x==['one', 'two'], 'array did not work')

assert(meson.has_external_property('anarray'), 'expected property "anarray" to exist')
assert(meson.has_external_property('astring'), 'expected property "astring" to exist')
assert(not meson.has_external_property('abool'), 'did not expect property "abool" to exist')

# These exist in both
assert(meson.has_external_property('anarray', native: false), 'FIXME')
assert(meson.has_external_property('anarray', native: true), 'FIXME')
assert(meson.has_external_property('astring', native: false), 'FIXME')
assert(meson.has_external_property('astring', native: true), 'FIXME')

if meson.is_cross_build()
  # This property only exists in the cross file
  assert(meson.has_external_property('red'), 'expected property "red" to exist in cross file')
  assert(meson.has_external_property('red', native: false), 'expected property "red" to exist in cross file')
  assert(not meson.has_external_property('red', native: true), 'did not expect property "red" to exist in native file')

  assert(not meson.has_external_property('abool', native: false), 'FIXME')
  assert(not meson.has_external_property('abool', native: false), 'FIXME')
else
  assert(not meson.has_external_property('red'), 'did not expect property "red" to exist in native file')
  assert(not meson.has_external_property('red', native: false), 'did not expect property "red" to exist in cross file because we are not doing a cross build')
  assert(not meson.has_external_property('red', native: true), 'did not expect property "red" to exist in native file')
endif
