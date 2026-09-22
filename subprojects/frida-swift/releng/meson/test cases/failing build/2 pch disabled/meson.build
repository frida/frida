# Disable PCH usage to make sure backends respect this setting.
# Since the .c file requires PCH usage (it does not include necessary
# headers itself), the build should fail.
project('pch test', 'c', default_options: ['b_pch=false'])
subdir('c')
