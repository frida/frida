project('valatest', 'c', 'vala')

glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')
gmodule_dep = dependency('gmodule-2.0')

foo_sources = ['plugin.vala', 'plugin-module.vala']
foo_lib = shared_library('foo', foo_sources,
                         dependencies: [glib_dep, gobject_dep, gmodule_dep])

shared_module('bar', 'plugin-bar.vala',
              dependencies: [glib_dep, gobject_dep],
              link_with: foo_lib)

foo_bin = executable('foo', 'foo.vala',
                     c_args: ['-DFOO_PLUGIN_PATH="@0@"'.format(meson.current_build_dir())],
                     dependencies: [glib_dep, gobject_dep],
                     link_with: foo_lib)

test('shared module', foo_bin)
