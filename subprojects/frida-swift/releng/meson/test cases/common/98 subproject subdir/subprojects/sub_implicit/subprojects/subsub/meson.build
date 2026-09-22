project('subsub')

meson.override_dependency('subsub', declare_dependency())

# Regression test: Installing a header from nested sub-subproject used to raise:
# ERROR: Sandbox violation: Tried to grab file foo.h from a nested subproject.
install_headers('foo.h')
