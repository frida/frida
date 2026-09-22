netif_sources = [
  'tunnel-interface-observer.vala',
]

extra_vala_args = []
extra_link_args = []

if host_os in ['ios', 'tvos']
  extra_vala_args += [
    '--pkg=corefoundation',
    '--pkg=darwin-gcd',
    '--pkg=darwin-systemconfiguration',
  ]
  extra_link_args += '-Wl,-framework,SystemConfiguration'
endif

netif = static_library('frida-netif', netif_sources,
  c_args: frida_component_cflags,
  vala_args: [base_vala_args, extra_vala_args],
  dependencies: [base_dep, gee_dep],
)

netif_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_args: extra_link_args,
  link_with: netif,
  dependencies: base_dep,
)
