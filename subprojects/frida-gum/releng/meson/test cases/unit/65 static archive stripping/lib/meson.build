project('lib', ['c'])

pkg = import('pkgconfig')

a = library('test-a', files('libA.c'), install: true)
install_headers(files('libA.h'), subdir: 'libA')
pkg.generate(
    a,
    version: '0.0',
    description: 'test library libA',
    filebase: 'test-a',
    name: 'test library libA',
    subdirs: 'libA')

b = static_library('test-b', files('libB.c'), install: true)
install_headers(files('libB.h'), subdir: 'libB')
pkg.generate(
    b,
    version: '0.0',
    description: 'test library libB',
    filebase: 'test-b',
    name: 'test library libB',
    subdirs: 'libB')
