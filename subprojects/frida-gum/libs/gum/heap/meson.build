gum_heap_headers = [
  'gumallocationblock.h',
  'gumallocationgroup.h',
  'gumallocationtracker.h',
  'gumallocatorprobe.h',
  'gumboundschecker.h',
  'gumcobject.h',
  'gumcobjecttracker.h',
  'guminstancetracker.h',
  'gumpagepool.h',
  'gumsanitychecker.h',
]

gum_heap_sources = [
  'gumallocationblock.c',
  'gumallocationgroup.c',
  'gumallocationtracker.c',
  'gumallocatorprobe.c',
  'gumboundschecker.c',
  'gumcobject.c',
  'gumcobjecttracker.c',
  'guminstancetracker.c',
  'gumpagepool.c',
  'gumsanitychecker.c',
]

install_headers(gum_heap_headers, subdir: install_header_subdir / 'heap')

gum_heap = library('frida-gum-heap-' + api_version, gum_heap_sources,
  c_args: frida_component_cflags,
  include_directories: gum_incdirs,
  dependencies: gum_dep,
  install: true,
)

gum_heap_dep = declare_dependency(link_with: gum_heap)

pkg = import('pkgconfig')
pkg.generate(filebase: 'frida-gum-heap-' + api_version,
  name: 'GumHeap',
  version: frida_version,
  description: 'Heap instrumentation library built on top of Gum',
  requires: ['frida-gum-' + api_version],
  subdirs: install_header_basedir,
  libraries: [gum_heap],
)

meson.override_dependency('frida-gum-heap-' + api_version, gum_heap_dep)
