project('foo', meson_version: '>= 0.1')

if meson.version().version_compare('>= 0.55')
  # This Should not produce warning even when using function not available in
  # meson 0.1.
  foo_dep = declare_dependency()
  meson.override_dependency('foo2', foo_dep)
endif
