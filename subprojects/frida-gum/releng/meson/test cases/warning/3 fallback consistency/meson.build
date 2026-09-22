project('proj', 'c')

# The first call succeed and cache the value of 'sub' dependency. The 2nd call
# should return the cached value, but still verify the fallback variable is
# consistent.
dependency('sub', fallback : ['sub', 'dep1'])
dependency('sub', fallback : ['sub', 'dep2'])
