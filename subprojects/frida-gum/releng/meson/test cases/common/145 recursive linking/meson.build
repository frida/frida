project('recursive dependencies', 'c')

# Test that you can link a shared executable to:
# - A shared library with no other deps
subdir('shnodep')
# - A static library with no other deps
subdir('stnodep')
# - A shared library with a shared library dep
subdir('shshdep')
# - A shared library with a static library dep
subdir('shstdep')
# - A static library with a shared library dep
subdir('stshdep')
# - A static library with a static library dep
subdir('ststdep')

test('alldeps',
     executable('alldeps', 'main.c',
                link_with : [shshdep, shstdep, ststdep, stshdep]))

# More combinations of static and shared libraries
subdir('3rdorderdeps')

# Circular dependencies between static libraries
# This requires the use of --start/end-group with GNU ld
subdir('circular')

# Various edge cases that have been reported
subdir('edge-cases')
