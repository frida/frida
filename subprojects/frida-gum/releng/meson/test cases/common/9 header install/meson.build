project('header install')

as_array = ['subdir.h', 'child/childdir.h']

subdir('vanishing_subdir')
subdir('sub')

h1 = install_headers('rootdir.h')
h2 = install_headers(as_array, subdir : 'subdir')
h3 = install_headers(as_array, subdir : 'subdir', preserve_path : true)
h4 = install_headers(subheader)
h5 = install_headers(disabler())
