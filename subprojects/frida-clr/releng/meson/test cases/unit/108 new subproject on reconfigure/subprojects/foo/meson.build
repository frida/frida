project('foo', 'c')

# Ensure that builtin options have been initialised.
assert(get_option('default_library') == 'shared')

# This uses default_library option internally and used to cause a crash:
# https://github.com/mesonbuild/meson/issues/10225.
library('foo', 'foo.c')
