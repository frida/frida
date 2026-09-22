project('gir link order', 'c')

if not dependency('glib-2.0', required : false).found() or not find_program('g-ir-scanner', required: false).found()
  error('MESON_SKIP_TEST glib not found.')
endif

gnome = import('gnome')
glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')

# get-prgname is a shared library which uses a function from glib-2.0. It is
# used to introduce external -L flags which may cause -L order problems.
subdir('get-prgname')

# fake-gthread is a shared library which has the same name as gthread-2.0 from
# GLib. This is used to simulate the case where an older or unrelated version
# of a library is already installed on the system. Our meson sample library
# defined below uses a function from fake-gthread. If meson messes up -L order,
# the linker will find libgthread-2.0.so installed on the system and fail to
# find the symbol our meson sample library uses.
subdir('fake-gthread')

meson_sample_sources = ['meson-sample.c', 'meson-sample.h']
meson_sample_lib = shared_library(
  'sample',
  sources : meson_sample_sources,
  dependencies : [gobject, get_prgname, fake_gthread],
  install : false,
)

gnome.generate_gir(
  meson_sample_lib,
  sources : meson_sample_sources,
  nsversion : '1.0',
  namespace : 'Meson',
  symbol_prefix : 'meson',
  identifier_prefix : 'Meson',
  includes : ['GObject-2.0'],
  install : false,
  build_by_default: true,
)
