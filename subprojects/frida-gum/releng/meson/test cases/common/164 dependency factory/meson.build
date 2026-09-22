project('dependency factory', 'c', meson_version : '>=0.53')

dep = dependency('gl', method: 'pkg-config', required: false)
if dep.found()
  assert(dep.type_name() == 'pkgconfig')
  dep.get_pkgconfig_variable('prefix')
endif

dep = dependency('SDL2', method: 'pkg-config', required: false)
if dep.found()
  assert(dep.type_name() == 'pkgconfig')
  dep.get_pkgconfig_variable('prefix')
endif

dep = dependency('SDL2', method: 'config-tool', required: false)
if dep.found()
  assert(dep.type_name() == 'config-tool')
  dep.get_configtool_variable('prefix')
endif

dep = dependency('Vulkan', method: 'pkg-config', required: false)
if dep.found()
  assert(dep.type_name() == 'pkgconfig')
  dep.get_pkgconfig_variable('prefix')
endif

dep = dependency('pcap', method: 'pkg-config', required: false)
if dep.found()
  assert(dep.type_name() == 'pkgconfig')
  dep.get_pkgconfig_variable('prefix')
endif

dep = dependency('pcap', method: 'config-tool', required: false)
if dep.found()
  assert(dep.type_name() == 'config-tool')
  dep.get_configtool_variable('prefix')
endif

dep = dependency('cups', method: 'pkg-config', required: false)
if dep.found()
  assert(dep.type_name() == 'pkgconfig')
  dep.get_pkgconfig_variable('prefix')
endif

dep = dependency('cups', method: 'config-tool', required: false)
if dep.found()
  assert(dep.type_name() == 'config-tool')
  dep.get_configtool_variable('prefix')
endif

dep = dependency('libwmf', method: 'pkg-config', required: false)
if dep.found()
  assert(dep.type_name() == 'pkgconfig')
  dep.get_pkgconfig_variable('prefix')
endif

dep = dependency('libwmf', method: 'config-tool', required: false)
if dep.found()
  assert(dep.type_name() == 'config-tool')
  dep.get_configtool_variable('prefix')
endif

dep = dependency('boost', method: 'system', required: false)
if dep.found()
  assert(dep.type_name() == 'system')
endif
