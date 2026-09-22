project('nested subproject regenerate depends', 'c')

if not find_program('cmake', required: false, version: '>=3.14').found()
  error('MESON_SKIP_TEST cmake >= 3.14 not available.')
endif

s = subproject('sub1')

# This is needed to make msbuild noop check work correctly
executable('exe', 'main.c')
