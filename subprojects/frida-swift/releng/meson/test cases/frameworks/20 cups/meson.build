project('cups test', 'c')

cups_dep = dependency('cups', version : '>=1.4', required: false)

if not cups_dep.found()
  error('MESON_SKIP_TEST cups not found.')
endif

e = executable('cups_prog', 'cups_prog.c', dependencies : cups_dep)

test('cupstest', e)

# ensure we can find the cups dependency via the legacy and modern config-tool
# options
depCC = dependency('cups', version : '>=1.4', method : 'cups-config')
depCT = dependency('cups', version : '>=1.4', method : 'config-tool')
depCM = dependency('cups', version : '>=1.4', method : 'cmake')

# check we can apply a version constraint
dependency('cups', version: '>=@0@'.format(depCT.version()), method: 'pkg-config', required: false)
dependency('cups', version: '>=@0@'.format(depCT.version()), method: 'config-tool')
