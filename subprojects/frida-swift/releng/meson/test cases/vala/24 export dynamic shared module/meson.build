project ('test', ['c', 'vala'], version: '0.1')

deps = [
  dependency ('glib-2.0'),
  dependency ('gmodule-2.0'),
]

app = executable (
  'app',
  ['app.vala'],
  dependencies: deps,
  export_dynamic: true
)

shared_module (
  'app_module',
  ['module.vala'],
  link_with: app,
  dependencies : deps,
)
