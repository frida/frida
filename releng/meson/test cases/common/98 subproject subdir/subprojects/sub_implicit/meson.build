project('sub_implicit', 'c', version : '1.0')

dep = declare_dependency()
meson.override_dependency('sub_implicit', dep)
meson.override_dependency('sub_implicit_provide1', dep)

# This one is not overridden but the wrap file tells the variable name to use.
sub_implicit_provide2_dep = dep

# This one is not overridden but the wrap file tells the variable name to use.
glib_dep = dep

assert(get_option('opt') == 'overridden')
