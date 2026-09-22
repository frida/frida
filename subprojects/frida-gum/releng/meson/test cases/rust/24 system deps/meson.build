project('system deps', 'rust')

glib = dependency('glib-2.0', required: false)
if not glib.found()
  error('MESON_SKIP_TEST: Need glib system dependency')
endif

rlib = static_library('wrapper', 'wrapper.rs', dependencies: glib)
exe = executable('main', 'main.rs', link_with: rlib)
