project('rust linking to c using dependency', 'c', 'rust')

if host_machine.system() == 'darwin'
  error('MESON_SKIP_TEST: does not work right on macos, please fix!')
endif

dep_zlib = dependency('zlib', static : get_option('static'), method : get_option('method'), required : false)
if not dep_zlib.found()
  error('MESON_SKIP_TEST: Could not find a @0@ zlib'.format(get_option('static') ? 'static' : 'shared'))
endif

l = static_library(
  'c_accessing_zlib',
  'c_accessing_zlib.c',
  dependencies: [dep_zlib],
)

e = executable(
  'prog', 'prog.rs',
  link_with : l,
)

test('cdepstest', e)
