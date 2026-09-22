project('simd', 'c')

simd = import('unstable-simd')

cc = meson.get_compiler('c')

cdata = configuration_data()

if not meson.is_cross_build() and host_machine.cpu_family() == 'arm' and cc.get_id() == 'clang'
  message('Adding -march=armv7 because assuming that this build happens on Raspbian.')
  message('Its Clang seems to be misconfigured and does not support NEON by default.')
  add_project_arguments('-march=armv7', language : 'c')
endif

if cc.get_id() == 'msvc' and cc.version().version_compare('<17')
  error('MESON_SKIP_TEST VS2010 produces broken binaries on x86.')
endif

# FIXME add [a, b] = function()
rval = simd.check('mysimds',
  mmx : 'simd_mmx.c',
  sse : 'simd_sse.c',
  sse2 : 'simd_sse2.c',
  sse3 : 'simd_sse3.c',
  ssse3 : 'simd_ssse3.c',
  sse41 : 'simd_sse41.c',
  sse42 : 'simd_sse42.c',
  avx : 'simd_avx.c',
  avx2 : 'simd_avx2.c',
  neon : 'simd_neon.c',
  compiler : cc,
  include_directories : include_directories('include'))

simdlibs = rval[0]
cdata.merge_from(rval[1])

configure_file(output : 'simdconfig.h',
  configuration : cdata)

p = executable('simdtest', 'simdchecker.c', 'fallback.c',
  link_with : simdlibs)

test('simdtest', p)
