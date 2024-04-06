project('frida', 'c',
  version: run_command('releng' / 'frida_version.py', check: true).stdout().strip(),
)

fs = import('fs')
python = import('python').find_installation()

is_cross_build = meson.is_cross_build()

gum_options = [
  'frida_version=' + meson.project_version(),
  'graft_tool=' + (get_option('graft_tool').disable_auto_if(is_cross_build).allowed() ? 'enabled' : 'disabled'),
  'gumjs=enabled',
]
subproject('frida-gum', default_options: gum_options)

core_options = [
  'frida_version=' + meson.project_version(),
  'portal=' + (get_option('portal').allowed() ? 'enabled' : 'disabled'),
]
foreach component : ['gadget', 'server', 'inject']
  core_options += component + '=' + (get_option(component).disable_auto_if(not is_cross_build).allowed() ? 'enabled' : 'disabled')
endforeach
subproject('frida-core', default_options: core_options)

grab_submodule = ['git', 'submodule', 'update', '--init', '--depth', '1']

if get_option('frida_clr') \
    .disable_auto_if(is_cross_build) \
    .disable_auto_if(build_machine.system() != 'windows') \
    .disable_auto_if(get_option('b_vscrt').startswith('mt')) \
    .allowed()
  dir = 'subprojects' / 'frida-clr'
  if not fs.exists(dir / 'meson.build')
    run_command(grab_submodule, dir, check: true)
  endif
  if get_option('frida_clr').auto()
    detect_result = run_command(python, dir / 'detect-netfx.py', check: false)
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
  dir = 'subprojects' / 'frida-node'
  if not fs.exists(dir / 'meson.build')
    run_command(grab_submodule, dir, check: true)
  endif
  subproject('frida-node')
endif

if get_option('frida_python') \
    .disable_auto_if(is_cross_build) \
    .allowed()
  dir = 'subprojects' / 'frida-python'
  if not fs.exists(dir / 'meson.build')
    run_command(grab_submodule, dir, check: true)
  endif
  subproject('frida-python')
endif

if get_option('frida_swift') \
    .disable_auto_if(is_cross_build) \
    .disable_auto_if(build_machine.system() != 'macos') \
    .allowed()
  dir = 'subprojects' / 'frida-swift'
  if not fs.exists(dir / 'meson.build')
    run_command(grab_submodule, dir, check: true)
  endif
  subproject('frida-swift')
endif

qt6_dep = dependency('qt6', modules: ['Core'], required: false)
if get_option('frida_qml') \
    .disable_auto_if(is_cross_build) \
    .disable_auto_if(not qt6_dep.found()) \
    .allowed()
  dir = 'subprojects' / 'frida-qml'
  if not fs.exists(dir / 'meson.build')
    run_command(grab_submodule, dir, check: true)
  endif
  subproject('frida-qml')
endif

if get_option('frida_tools') \
    .disable_auto_if(is_cross_build) \
    .allowed()
  dir = 'subprojects' / 'frida-tools'
  if not fs.exists(dir / 'meson.build')
    run_command(grab_submodule, dir, check: true)
  endif
  subproject('frida-tools')
endif
