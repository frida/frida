project('libgcrypt test', 'c')

wm = find_program('libgcrypt-config', required : false)
if not wm.found()
  error('MESON_SKIP_TEST: libgcrypt-config not installed')
endif

libgcrypt_dep = dependency('libgcrypt', version : '>= 1.0')
libgcrypt_ver = libgcrypt_dep.version()
assert(libgcrypt_ver.split('.').length() > 1, 'libgcrypt version is "@0@"'.format(libgcrypt_ver))
message('libgcrypt version is "@0@"'.format(libgcrypt_ver))
e = executable('libgcrypt_prog', 'libgcrypt_prog.c', dependencies : libgcrypt_dep)

test('libgcrypttest', e)

# Test using the method keyword:

dependency('libgcrypt', method : 'config-tool')
dependency('libgcrypt', method : 'pkg-config', required: false)

# Check we can apply a version constraint
dependency('libgcrypt', version: '>=@0@'.format(libgcrypt_dep.version()), method: 'pkg-config', required: false)
dependency('libgcrypt', version: '>=@0@'.format(libgcrypt_dep.version()), method: 'config-tool')
