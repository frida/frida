project('composite', 'vala', 'c')
gnome = import('gnome')
deps = [
  dependency('glib-2.0', version : '>=2.38'),
  dependency('gobject-2.0'),
  dependency('gtk+-3.0'),
]
res = files('my-resources.xml')
gres = gnome.compile_resources(
  'my', res,
  source_dir : '.',
)
executable(
  'demo',
  sources : [
    'mywidget.vala',
    gres,
  ],
  dependencies : deps,
  vala_args : ['--gresources', res],
)
