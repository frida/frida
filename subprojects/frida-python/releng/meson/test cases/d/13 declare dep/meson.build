project('meson-d-sample', 'd',
    version: '0.1.0',
)

my_dep = declare_dependency(
    d_module_versions: ['TestVersion'],
    d_import_dirs: include_directories('views'),
)

test_exe = executable('test-dep', 'test.d',
    dependencies: my_dep,
)
test('test d features in declare_dependency', test_exe)
