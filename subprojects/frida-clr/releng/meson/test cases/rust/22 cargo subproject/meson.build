project('cargo subproject', 'c')

foo_dep = dependency('foo-0-rs', native : true)
exe = executable('app_native', 'main.c',
  dependencies: foo_dep,
  native : true,
)

foo_dep = dependency('foo-0-rs')
exe = executable('app', 'main.c',
  dependencies: foo_dep,
)
test('cargo-test', exe)
