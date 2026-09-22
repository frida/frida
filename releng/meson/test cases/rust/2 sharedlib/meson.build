project('rust shared library', 'rust', 'c')

if host_machine.system() == 'darwin'
  error('MESON_SKIP_TEST: does not work right on macos, please fix!')
endif

s = static_library('static', 'value.c')
l = shared_library('stuff', 'stuff.rs', link_whole : s, install : true)
e = executable('prog', 'prog.rs', link_with : l, install : true)

if build_machine.system() == 'windows'
  rustup = find_program('rustup')
  test('linktest', rustup,
       args: ['run', 'stable', e])
else
  test('linktest', e)
endif
