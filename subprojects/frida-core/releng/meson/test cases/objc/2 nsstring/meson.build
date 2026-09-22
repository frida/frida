project('nsstring', 'objc')

if host_machine.system() == 'darwin'
  dep = dependency('appleframeworks', modules : 'Foundation')
elif host_machine.system() == 'cygwin'
  error('MESON_SKIP_TEST GNUstep is not packaged for Cygwin.')
else
  dep = dependency('gnustep', required : false)
  if not dep.found()
    error('MESON_SKIP_TEST: GNUstep is not installed')
  endif
  if host_machine.system() == 'linux' and meson.get_compiler('objc').get_id() == 'clang'
    error('MESON_SKIP_TEST: GNUstep is broken on Linux with Clang')
  endif
endif
exe = executable('stringprog', 'stringprog.m', dependencies : dep)
test('stringtest', exe)

# Ensure that a non-required dep that is not found does not cause an error
dependency('appleframeworks', modules: 'nonexisting', required: false)
