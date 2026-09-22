project('frida-node', 'c',
  version: run_command(find_program('python3'), files('scripts' / 'detect-version.py'),
    capture: true,
    check: true).stdout().strip(),
  meson_version: '>=1.1.0',
)

cc = meson.get_compiler('c')
strip = (cc.get_argument_syntax() == 'msvc') ? '' : find_program('strip')

node = find_program('node')
npm = find_program('npm')
python = import('python').find_installation()

version = meson.project_version()

system = host_machine.system()

napi_version = 8

node_platforms = {
  'windows': 'win32',
}
node_platform = node_platforms.get(system, system)

gyp_oses = {
  'windows': 'win',
  'darwin': 'mac',
}
gyp_os = gyp_oses.get(system, system)

gyp_archs = {
  'x86': 'ia32',
  'x86_64': 'x64',
  'arm': 'armv7l',
  'aarch64': 'arm64',
}
cpu_family = host_machine.cpu_family()
gyp_arch = gyp_archs.get(cpu_family, cpu_family)

message('Fetching ABI bits')
metadata = run_command(python, files('scripts' / 'fetch-abi-bits.py'),
  gyp_os, gyp_arch, node, npm, meson.current_build_dir(),
  capture: true,
  check: true,
).stdout().strip().split('\n')
node_defines = []
node_incdirs = []
node_libs = []
foreach item : metadata
  if item.startswith('node_defines: ')
    node_defines += item.substring(14).split(' ')
  elif item.startswith('node_incdir: ')
    node_incdirs += item.substring(13)
  elif item.startswith('node_lib: ')
    node_libs += meson.current_build_dir() / item.substring(10)
  else
    error('Unexpected API bits item:', item)
  endif
endforeach

extra_c_args = []
extra_link_args = []
extra_link_depends = []

foreach d : node_defines
  extra_c_args += '-D' + d
endforeach

if host_machine.system() == 'windows'
  extra_c_args += [
    '/wd4244',
  ]
  extra_link_args += [
    '/DELAYLOAD:node.exe',
    '-lDelayImp',
  ]
endif

frida_core_dep = dependency('frida-core-1.0', default_options: [
  f'frida_version=@version@',
])

pkg_install_dir = get_option('libdir') / 'node_modules' / 'frida'

package_json = files('package.json')
package_lock_json = files('package-lock.json')
tsconfig_json = files('tsconfig.json')

subdir('src')

binding = shared_module('frida_binding', frida_binding_sources,
  name_prefix: '',
  name_suffix: 'node',
  include_directories: include_directories(node_incdirs),
  implicit_include_directories: false,
  c_args: [
    '-DBUILDING_NODE_EXTENSION',
    '-DNODE_GYP_MODULE_NAME=frida_binding',
    f'-DNAPI_VERSION=@napi_version@',
  ] + extra_c_args,
  link_args: node_libs + extra_link_args,
  link_depends: extra_link_depends,
  dependencies: frida_core_dep,
  install: true,
  install_dir: pkg_install_dir / 'build',
  vs_module_defs: 'src' / 'addon.def',
)

custom_target('prebuild',
  input: [binding],
  output: [f'frida-v@version@-napi-v@napi_version@-@node_platform@-@gyp_arch@.tar.gz'],
  command: [
    python,
    files('scripts' / 'package.py'),
    '>>>', strip, '<<<',
    get_option('strip').to_string(),
    '@INPUT@',
    '@OUTPUT@',
  ],
)
