project('framework dependency with version', 'c')

if host_machine.system() != 'darwin'
  error('MESON_SKIP_TEST test only applicable on darwin')
endif

# do individual frameworks have a meaningful version to test?  And multiple frameworks might be listed...
dep = dependency('appleframeworks', modules: 'Foundation', version: '>0')
