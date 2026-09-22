project('include order', 'c')

# Test that the order of priority of include paths (from first to last) is:
#
# 1. Target's current build directory
# 2. Target's current source directory
# 3. Include paths added with the `c_args:` kwarg
# 4. Include paths added with the `include_directories`: kwarg
#    Within this, the build dir takes precedence over the source dir
# 5. Include paths added via `include_directories:` of internal deps
#    Within this, the build dir takes precedence over the source dir

# Custom target dir with a built header
subdir('ctsub')
# Configures a header file
subdir('inc3')
# Defines an internal dep
subdir('sub1')
# Defines a per-target include path
subdir('sub2')
# Directory for `c_args:` include path
subdir('sub3')
# The directory where the target resides
subdir('sub4')

# Test that the order in which internal dependencies are specified is
# preserved. This is needed especially when subprojects get involved and
# multiple build-root config.h files exist, and we must be sure that the
# correct one is found: https://github.com/mesonbuild/meson/issues/1495
f = executable('somefxe', 'sub4/main.c',
               dependencies : [correctinc, dep, wronginc])

test('eh', e)
test('oh', f)

# Test that the order in include_directories() is maintained
incs = include_directories('inc1', 'inc2', 'inc3')
executable('ordertest', 'ordertest.c', include_directories: incs)
