project('demo', 'c', 'vala')

gnome = import('gnome', required: false)

if not gnome.found()
  error('MESON_SKIP_TEST: gnome module not supported')
endif

deps = [
  dependency('glib-2.0', version : '>=2.50'),
  dependency('gobject-2.0'),
  dependency('gtk+-3.0'),
]

ui_tgt = custom_target(
  input: 'TestBox.ui.in',
  output:  'TestBox.ui',
  command: [find_program('cat')],
  feed: true,
  capture: true,
)

resources = gnome.compile_resources('test-resources',
  'test.gresource.xml',
  c_name: 'test_res',
  dependencies: ui_tgt,
)

executable(
  'demo',
  'test.vala',
  resources,
  dependencies: deps,
)
