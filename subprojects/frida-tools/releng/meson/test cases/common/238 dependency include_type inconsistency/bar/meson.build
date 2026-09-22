baz_dep = dependency('baz',
    fallback: ['baz', 'baz_dep'],
    include_type: 'system',
    method: 'pkg-config', # if we comment this out or change to 'auto' the build is successful
    required: false)
