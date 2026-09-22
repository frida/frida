project('run always', 'c')

version = '1.0.0'

vgen = find_program('version_gen.py')

version_src = custom_target('Version string',
input : 'version.c.in',
output : 'version.c',
command : [vgen, '@INPUT@', '@OUTPUT@', version],
build_always : true,
)

executable('versionprinter', 'main.c', version_src)
