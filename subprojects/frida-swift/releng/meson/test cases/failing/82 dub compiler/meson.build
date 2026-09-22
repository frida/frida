project('dub')

if not add_languages('d', required: false)
  error('MESON_SKIP_TEST test requires D compiler')
endif

if meson.get_compiler('d').get_id() == 'dmd'
  if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
    error('MESON_SKIP_TEST Windows test environment lacks multiple D compilers.')
  endif
endif

if not find_program('dub', required: false).found()
  error('MESON_SKIP_TEST test requires dub')
endif

dependency('dubtestproject:test2', method: 'dub') # Compiler mismatch
