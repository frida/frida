project('impossible-dep-test', 'c', version : '1.0')

if not dependency('zlib', required: false).found()
  error('MESON_SKIP_TEST test requires zlib')
endif

dependency('zlib', version : ['>=1.0', '<1.0'])
