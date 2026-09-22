project('boosttest', 'cpp',
  default_options : ['cpp_std=c++11'])

if not dependency('boost', required: false).found()
  error('MESON_SKIP_TEST test requires boost')
endif

# abc doesn't exist
linkdep = dependency('boost', modules : ['thread', 'system', 'test', 'abc'])
