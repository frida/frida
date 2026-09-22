project('mixed dependence', 'vala', 'c')

cc = meson.get_compiler('c')

deps = [dependency('glib-2.0'), dependency('gobject-2.0'),
        # Should be ignored, see https://github.com/mesonbuild/meson/issues/1939
        cc.find_library('z')]

mixer = static_library('mixer', 'mixer.vala', 'mixer-glue.c',
  dependencies : deps)

app = executable('app', 'app.vala',
  link_with : mixer,
  dependencies : deps)

test('valamixeddependencetest', app)
