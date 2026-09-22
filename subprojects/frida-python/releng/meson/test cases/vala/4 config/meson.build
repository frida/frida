project('valatest', 'vala', 'c')

valac = meson.get_compiler('vala')
# Try to find our library
valadeps = [valac.find_library('meson-something-else', dirs : meson.current_source_dir())]
valadeps += [dependency('glib-2.0'), dependency('gobject-2.0')]

e = executable(
'valaprog',
sources : ['config.vapi', 'prog.vala'],
dependencies : valadeps,
c_args : ['-DDATA_DIRECTORY="@0@"'.format(meson.current_source_dir()),
          '-DSOMETHING_ELSE="Out of this world!"']
)
test('valatest', e)
