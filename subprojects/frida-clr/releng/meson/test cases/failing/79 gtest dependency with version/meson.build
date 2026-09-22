project('gtest dependency with version', 'cpp')

if not dependency('gtest', method: 'system', required: false).found()
   error('MESON_SKIP_TEST test requires gtest')
endif

# discovering gtest version is not yet implemented
dep = dependency('gtest', method: 'system', version: '>0')
