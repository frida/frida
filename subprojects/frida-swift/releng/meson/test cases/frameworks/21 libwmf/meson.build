project('libwmf test', 'c')

wm = find_program('libwmf-config', required : false)
if not wm.found() or meson.is_cross_build()
  error('MESON_SKIP_TEST: libwmf-config not installed')
endif

libwmf_dep = dependency('libwmf', version : '>= 0.2.8')
libwmf_ver = libwmf_dep.version()
assert(libwmf_ver.split('.').length() > 1, 'libwmf version is "@0@"'.format(libwmf_ver))
message('libwmf version is "@0@"'.format(libwmf_ver))
# Workaround for Debian bug 912563 where libwmf-devel returns cflags
# that do not have Freetype include paths but their headers
# use them unconditionally.
ft_dep = dependency('freetype2')
e = executable('libwmf_prog', 'libwmf_prog.c', dependencies : [libwmf_dep, ft_dep])

test('libwmftest', e)

# Test using the method keyword:

dependency('libwmf', method : 'config-tool')
dependency('libwmf', method : 'libwmf-config')

# Check we can apply a version constraint
dependency('libwmf', version: '>=@0@'.format(libwmf_dep.version()), method: 'pkg-config', required: false)
dependency('libwmf', version: '>=@0@'.format(libwmf_dep.version()), method: 'config-tool')
