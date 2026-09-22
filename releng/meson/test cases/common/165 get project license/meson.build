project('bar', 'c', license: 'Apache-2.0')

executable('bar', 'bar.c')

license = meson.project_license()[0]
if license != 'Apache-2.0'
    error('The license should be Apache-2.0, but it is: ' + license)
endif
