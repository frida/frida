project('resources', ['java'])

if meson.backend() != 'ninja'
  error('MESON_SKIP_TEST: only valid on backends which support jar()')
endif

subdir('src')
