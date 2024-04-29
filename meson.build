project('frida', 'c',
  version: run_command('releng' / 'frida_version.py', check: true).stdout().strip(),
  meson_version: '>=1.1.0',
)

python = import('python').find_installation()

is_cross_build = meson.is_cross_build()
is_watchos = host_machine.subsystem().split('-')[0] == 'watchos'

gum_options = [
  'frida_version=' + meson.project_version(),
  'graft_tool=' + (get_option('graft_tool').disable_auto_if(is_cross_build).allowed() ? 'enabled' : 'disabled'),
  'gumjs=enabled',
]
subproject('frida-gum', default_options: gum_options)

core_options = [
  'frida_version=' + meson.project_version(),
]
core_options += 'gadget=' + (get_option('gadget').disable_auto_if(not is_cross_build).allowed() ? 'enabled' : 'disabled')
foreach component : ['server', 'portal', 'inject']
  core_options += component + '=' + (get_option(component)
    .disable_auto_if(not is_cross_build)
    .disable_auto_if(is_watchos)
    .allowed() ? 'enabled' : 'disabled')
endforeach
subproject('frida-core', default_options: core_options)

ensure_submodules = [python, files('tools' / 'ensure-submodules.py')]

if get_option('frida_clr') \
    .disable_auto_if(is_cross_build) \
    .disable_auto_if(build_machine.system() != 'windows') \
    .disable_auto_if(get_option('b_vscrt').startswith('mt')) \
    .allowed()
  run_command(ensure_submodules, 'frida-clr', check: true)
  if get_option('frida_clr').auto()
    detect_result = run_command(python, 'subprojects' / 'frida-clr' / 'detect-netfx.py', check: false)
    build_clr_bindings = detect_result.returncode() == 0
  else
    build_clr_bindings = true
  endif
  if build_clr_bindings
    subproject('frida-clr')
  endif
endif

if get_option('frida_node') \
    .disable_auto_if(is_cross_build) \
    .allowed()
  run_command(ensure_submodules, 'frida-node', check: true)
  subproject('frida-node')
endif

if get_option('frida_python') \
    .disable_auto_if(is_cross_build) \
    .allowed()
  run_command(ensure_submodules, 'frida-python', check: true)
  subproject('frida-python')
endif

if get_option('frida_swift') \
    .disable_auto_if(is_cross_build) \
    .disable_auto_if(build_machine.system() != 'macos') \
    .allowed()
  run_command(ensure_submodules, 'frida-swift', check: true)
  subproject('frida-swift')
endif

if get_option('frida_qml') \
    .disable_auto_if(is_cross_build) \
    .allowed()
  qt6_dep = dependency('qt6', modules: ['Core'], required: false)
  if get_option('frida_qml') \
      .disable_auto_if(not qt6_dep.found()) \
      .allowed()
    run_command(ensure_submodules, 'frida-qml', check: true)
    subproject('frida-qml')
  endif
endif

if get_option('frida_tools') \
    .disable_auto_if(is_cross_build) \
    .allowed()
  run_command(ensure_submodules, 'frida-tools', check: true)
  subproject('frida-tools')
endif
