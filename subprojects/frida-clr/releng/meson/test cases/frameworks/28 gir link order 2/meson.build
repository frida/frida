project('gir link order 2', 'c')

if not dependency('gobject-2.0', required : false).found() or not find_program('g-ir-scanner', required: false).found()
  error('MESON_SKIP_TEST gobject not found.')
endif

gnome = import('gnome')
gobject = dependency('gobject-2.0')

# This builds a dummy libsample under samelibname that's not really used
subdir('samelibname')

# This builds the real libsample
meson_sample_sources = ['meson-sample.c', 'meson-sample.h']
meson_sample_lib = shared_library(
  'sample',
  sources : meson_sample_sources,
  dependencies : [gobject],
  link_with: [samelibname],
  install : false,
)

# g-ir-scanner should get the linker paths in the right order so it links first
# against the target libsample and not the dummy one that's just a dependency
# https://github.com/mesonbuild/meson/pull/5423
gnome.generate_gir(
  meson_sample_lib,
  sources : meson_sample_sources,
  nsversion : '1.0',
  namespace : 'Meson',
  symbol_prefix : 'meson',
  identifier_prefix : 'Meson',
  install : false,
  build_by_default: true,
)
