project('slashname', 'c')

# Traverse this subdir so the corresponding dir
# is created inside the build dir.
subdir('sub')

# Try to create an executable that would go in the "sub" dir
# inside the build dir. This is prohibited.
executable('sub/prog', pf)

error('Re-enable me once slash in name is finally prohibited.')
