project('gl dependency with version', 'c')

host_system = host_machine.system()
if host_system != 'windows' and host_system != 'darwin'
  error('MESON_SKIP_TEST: test only fails on Windows and OSX')
endif

# gl dependency found via system method doesn't have a meaningful version to check
dep = dependency('gl', method: 'system', version: '>0')
