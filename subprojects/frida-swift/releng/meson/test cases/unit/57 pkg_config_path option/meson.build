project('pkg_config_path option')

build = dependency('totally_made_up_dep', native: true, method : 'pkg-config')
host = dependency('totally_made_up_dep', native: false, method : 'pkg-config')

assert(build.version() == '4.5.6', 'wrong version for build machine dependency')
assert(host.version() == '1.2.3', 'wrong version for host machine dependency')
