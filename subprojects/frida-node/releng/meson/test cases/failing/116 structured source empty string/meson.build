project('structured_source with empty string key')

if not add_languages(['rust'], required : false, native : false)
  error('MESON_SKIP_TEST: Rust is required but not found.')
endif

executable(
  'main',
  structured_sources(
    'main.rs',
    {'' : 'main.rs'},
  )
)
