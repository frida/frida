project('version compare', meson_version: '>= 0.1')

if meson.version().version_compare('>= 9999')
  error('This should not be executed')
elif meson.version().version_compare('>= 0.55') and false
  error('This should not be executed')
elif not meson.version().version_compare('>= 0.55')
  error('This should not be executed')
elif meson.version().version_compare('>= 0.55')
  # This Should not produce warning even when using function not available in
  # meson 0.1.
  foo_dep = declare_dependency()
  meson.override_dependency('foo', foo_dep)
endif

# This will error out if elif cause did not enter
assert(foo_dep.found(), 'meson.version_compare did not work')

subproject('foo')
