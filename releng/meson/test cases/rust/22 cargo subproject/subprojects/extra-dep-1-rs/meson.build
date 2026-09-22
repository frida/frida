project('extra dep', 'c', version: '1.0')

assert(get_option('feature-default') == true)

l = static_library('extra-dep', 'lib.c')
d = declare_dependency(link_with: l,
  variables: {
    'features': 'default',
  })
meson.override_dependency('extra-dep-1-rs', d)
