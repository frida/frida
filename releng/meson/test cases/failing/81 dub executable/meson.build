project('dub')

if not add_languages('d', required: false)
  error('MESON_SKIP_TEST test requires D compiler')
endif

if not find_program('dub', required: false).found()
  error('MESON_SKIP_TEST test requires dub')
endif

dependency('dubtestproject:test1', method: 'dub') # Not library (executable)
