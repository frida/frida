project('same basename', 'c')

subdir('sharedsub')
subdir('staticsub')

# Use the same source file to check that each top level target
# has its own unique working directory. If they don't
# then the .o files will clobber each other.

exe1 = executable('name', 'exe1.c', link_with : stlib)
exe2 = executable('name2', 'exe2.c', link_with : shlib)

test('static', exe1)
test('shared', exe2)
