project('pcap test', 'c')

pcap_dep = dependency('pcap', version : '>=1.0', required: false)

if not pcap_dep.found()
  error('MESON_SKIP_TEST pcap not found.')
endif

pcap_ver = pcap_dep.version()
assert(pcap_ver.split('.').length() > 1, 'pcap version is "@0@"'.format(pcap_ver))

e = executable('pcap_prog', 'pcap_prog.c', dependencies : pcap_dep)

test('pcaptest', e)

# Ensure discovery via the configuration tools work also
pcap_dep = dependency('pcap', version : '>=1.0', method : 'pcap-config')
pcap_dep = dependency('pcap', version : '>=1.0', method : 'config-tool')

# Check we can apply a version constraint
dependency('pcap', version: '>=@0@'.format(pcap_dep.version()), method: 'pkg-config', required: false)
dependency('pcap', version: '>=@0@'.format(pcap_dep.version()), method: 'config-tool')
