project('foo', 'c', 'vala')

glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
g_ir_compiler = find_program('g-ir-compiler')

foo = shared_library('foo', 'foo.vala',
                     install : true,
                     install_dir : [true, false, false, true],
                     vala_gir: 'Foo-1.0.gir',
                     dependencies: [glib, gobject])

custom_target('foo-typelib',
              command: [g_ir_compiler, '--output', '@OUTPUT@', '@INPUT@'],
              input: meson.current_build_dir() + '/Foo-1.0.gir',
              output: 'Foo-1.0.typelib',
              depends: foo)
