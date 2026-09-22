project('lib', ['c'])

a = library('test-a', 'liba.c', install: true)

b = library('test-b', 'libb.c', link_with: a, install: true)

import('pkgconfig').generate(
    version: '0.0',
    description: 'test library',
    filebase: 'test-b',
    name: 'test library',
    libraries: [b],
    subdirs: ['.']
)
