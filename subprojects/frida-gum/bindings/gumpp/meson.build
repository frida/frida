gumpp_headers = [
  'gumpp.hpp',
]

gumpp_sources = [
  'backtracer.cpp',
  'interceptor.cpp',
  'invocationlistener.cpp',
  'profiler.cpp',
  'returnaddress.cpp',
  'runtime.cpp',
  'sampler.cpp',
  'sanitychecker.cpp',
  'symbolutil.cpp',
]

extra_link_args = []
extra_link_depends = []
if host_os_family == 'darwin'
  symlist = 'gumpp.symbols'
  extra_link_args += ['-Wl,-exported_symbols_list,' + (meson.current_source_dir() / symlist)]
  extra_link_depends += [symlist]
elif host_os_family != 'windows'
  symscript = 'gumpp.version'
  extra_link_args += ['-Wl,--version-script,' + (meson.current_source_dir() / symscript)]
  extra_link_depends += [symscript]
endif

install_headers(gumpp_headers, subdir: install_header_basedir / 'gumpp')

gumpp = shared_library('frida-gumpp-' + api_version, gumpp_sources,
  c_args: frida_component_cflags,
  override_options: [
    'cpp_std=c++14',
    'cpp_eh=default',
    'cpp_rtti=false',
  ],
  link_args: extra_link_args,
  link_depends: extra_link_depends,
  vs_module_defs: 'gumpp.def',
  dependencies: [gum_dep, gum_heap_dep, gum_prof_dep],
  install: true,
)

gumpp_dep = declare_dependency(link_with: gumpp,
  include_directories: include_directories('.'),
)

pkg = import('pkgconfig')
pkg.generate(filebase: 'frida-gumpp-' + api_version,
  name: 'Gum++',
  version: frida_version,
  description: 'Gum C++ bindings',
  requires_private: ['frida-gum-' + api_version],
  subdirs: install_header_basedir,
  libraries: [gumpp],
)
