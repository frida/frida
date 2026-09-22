project('native subproject', 'c', meson_version : '>= 1.3.0')

# Test running a subproject for both host and build
subproject('both', native : true)
subproject('both', native : false)

maingen = executable('maingen', 'maingen.c', native : true)
meson.override_find_program('maingen', maingen, native : true)
gen = subproject('buildtool', native : true).get_variable('e')

ct = custom_target(
  'generated',
  command : [gen],
  output : 'generated.c',
  capture : true,
)

e = executable('testprog', ct, install : true)

test('main', e)

if meson.is_cross_build()
  subproject('test installs', native : true)
endif

# Test a subproject configured from the main project, and then called as a nested subproject
subproject('recursive-both', native : true)
subproject('recursive-both', native : false)

subproject('recursive-build-only', native : true)
subproject('recursive-host-only', native : false)
