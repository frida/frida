project('foo')

if meson.is_cross_build()
  error('MESON_SKIP_TEST Test does not make sense for cross builds')
endif

dep_zlib = dependency('zlib', required : false)
if not dep_zlib.found()
  error('MESON_SKIP_TEST Test requires zlib')
endif
dependency('zlib', native : true, required : false)
dependency('zlib', native : false)

# `native: true` should not make a difference when doing a native build.
meson.override_dependency('expat', declare_dependency())
dependency('expat')
dependency('expat', native : true)
dependency('expat', native : false)
