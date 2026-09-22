libbar_sources = [
  'bar.c',
  'bar.h',
]

libbar_deps = [
  dependency('gobject-2.0'),
  libfoo_dep,
]

libbar = shared_library('bar', libbar_sources,
  dependencies: libbar_deps,
  install: true,
)

libbar_api_ver = '1.0'

libbar_gir = gnome.generate_gir(libbar,
  sources: libbar_sources,
  namespace: 'Bar',
  nsversion: libbar_api_ver,
  symbol_prefix: 'bar',
  extra_args: [
    '--c-include=bar.h',
  ],
)

libbar_vapi = gnome.generate_vapi('bar-' + libbar_api_ver,
  sources: libbar_gir[0],
  packages: libfoo_vapi,
  install: true,
)
