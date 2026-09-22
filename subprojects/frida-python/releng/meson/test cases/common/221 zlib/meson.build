project('zlib system dependency', 'c')

if not ['darwin', 'freebsd', 'dragonfly', 'windows', 'android'].contains(host_machine.system())
  error('MESON_SKIP_TEST only applicable on macOS, FreeBSD, DragonflyBSD, Windows, and Android.')
endif

cc = meson.get_compiler('c')

if host_machine.system() == 'darwin' and cc.get_id() != 'clang'
  # this will only work on mac if using Apple's clang compiler, but there is no
  # way in the meson source level to differentiate apple clang and llvm clang
  # In the meson CI only apple clang is tested
  error('MESON_SKIP_TEST on macOS only clang is supported.')
endif

if not (cc.find_library('z', required: false).found() or
        cc.find_library('zlib', required : false).found() or
        cc.find_library('zlib1', required : false).found())
  error('MESON_SKIP_TEST Cannot seem to find zlib via find_library, this test will probably fail.')
endif

z = dependency('zlib', method : 'system')
assert(z.version().version_compare('>= 1.2'), 'Version does not seem to have been detected correctly.')
