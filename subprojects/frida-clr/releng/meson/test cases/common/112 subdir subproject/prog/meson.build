subproject('sub')
libSub = dependency('sub', fallback: ['sub', 'libSub'])

exe = executable('prog', 'prog.c', dependencies: libSub)
test('subdir subproject', exe)
