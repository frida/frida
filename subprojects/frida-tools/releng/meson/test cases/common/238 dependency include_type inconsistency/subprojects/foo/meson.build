project('foo')

baz_dep = dependency('baz',
    fallback: ['baz', 'baz_dep'],
    include_type: 'system',
    method: 'pkg-config',
    required: false)

foo_dep = declare_dependency(dependencies: baz_dep)
