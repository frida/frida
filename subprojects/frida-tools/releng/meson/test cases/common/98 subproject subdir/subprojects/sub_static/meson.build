project('sub_static')

assert(get_option('default_library') == 'static')
meson.override_dependency('sub_static', declare_dependency())
meson.override_dependency('sub_static2', declare_dependency(), static: true)
meson.override_dependency('sub_static3', declare_dependency(variables: {'static': 'true'}), static: true)
meson.override_dependency('sub_static3', declare_dependency(variables: {'static': 'false'}), static: false)

